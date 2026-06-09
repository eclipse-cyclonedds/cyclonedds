/*
 * Copyright(c) 2026 ZettaScale Technology and others
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */

#include "fuzz_type_graph.pb.h"

#include <src/libfuzzer/libfuzzer_macro.h>

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <vector>

extern "C" {
#include "dds/dds.h"
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/string.h"
#include "dds/ddsi/ddsi_domaingv.h"
#include "dds/ddsi/ddsi_entity.h"
#include "dds/ddsi/ddsi_entity_index.h"
#include "dds/ddsi/ddsi_iid.h"
#include "dds/ddsi/ddsi_init.h"
#include "dds/ddsi/ddsi_typelib.h"
#include "dds/ddsi/ddsi_typewrap.h"
#include "ddsi__thread.h"
#include "ddsi__typelib.h"
#include "ddsi__typewrap.h"
#include "ddsi__xt_impl.h"
}

namespace {

constexpr uint32_t kMaxTypes = 24;
constexpr uint32_t kMaxMembers = 8;
constexpr uint32_t kMaxValues = 8;
constexpr uint32_t kMaxActions = 8;
constexpr uint32_t kMaxMutations = 8;
constexpr int kActionRoundtripTypeInfoTypeMap = fuzz_type_graph::ACTION_ROUNDTRIP_TYPEINFO_TYPEMAP;

struct OwnedType {
  ddsi_type type;
  std::vector<xt_struct_member> struct_members;
  std::vector<xt_union_member> union_members;
  std::vector<std::vector<int32_t>> union_labels;
  std::vector<xt_enum_literal> enum_literals;
  std::vector<xt_bitflag> bitflags;
  std::vector<DDS_XTypes_LBound> bounds;
};

struct SccPairSet {
  std::vector<DDS_XTypes_TypeIdentifierTypeObjectPair> pairs;
  uint32_t seq_length = 0;
};

struct SerializedTypeInfoTypeMap {
  unsigned char *typeinfo = nullptr;
  uint32_t typeinfo_size = 0;
  unsigned char *typemap = nullptr;
  uint32_t typemap_size = 0;
};

static ddsi_cfgst *g_cfgst;

static void null_log_sink(void *varg, const dds_log_data_t *msg)
{
  (void) varg;
  (void) msg;
}

static uint32_t bounded_count(int n, uint32_t max)
{
  return std::min<uint32_t>(static_cast<uint32_t>(std::max(n, 0)), max);
}

static uint32_t bounded_index(uint32_t x, uint32_t n)
{
  return n == 0 ? 0 : x % n;
}

static bool is_hash_type(uint8_t kind)
{
  return kind == DDS_XTypes_TK_STRUCTURE || kind == DDS_XTypes_TK_UNION ||
    kind == DDS_XTypes_TK_ALIAS || kind == DDS_XTypes_TK_ENUM ||
    kind == DDS_XTypes_TK_BITMASK;
}

static DDS_XTypes_StructTypeFlag valid_aggregate_flags(uint32_t flags)
{
  static const DDS_XTypes_StructTypeFlag extensibility[] = {
    DDS_XTypes_IS_FINAL,
    DDS_XTypes_IS_APPENDABLE,
    DDS_XTypes_IS_MUTABLE
  };
  DDS_XTypes_StructTypeFlag out = extensibility[flags % 3u];
  if (flags & 0x10u)
    out |= DDS_XTypes_IS_NESTED;
  if (flags & 0x20u)
    out |= DDS_XTypes_IS_AUTOID_HASH;
  return out;
}

static DDS_XTypes_EnumTypeFlag valid_enum_flags(uint32_t flags)
{
  return (flags & 1u) ? DDS_XTypes_IS_APPENDABLE : DDS_XTypes_IS_FINAL;
}

static uint16_t valid_try_construct(uint32_t flags)
{
  uint16_t tc = static_cast<uint16_t>(flags & (DDS_XTypes_TRY_CONSTRUCT1 | DDS_XTypes_TRY_CONSTRUCT2));
  return tc == 0 ? DDS_XTypes_TRY_CONSTRUCT1 : tc;
}

static DDS_XTypes_StructMemberFlag valid_struct_member_flags(uint32_t flags)
{
  DDS_XTypes_StructMemberFlag out = valid_try_construct(flags);
  if (flags & 0x04u)
    out |= DDS_XTypes_IS_EXTERNAL;
  if (flags & 0x08u)
    out |= DDS_XTypes_IS_OPTIONAL;
  if (flags & 0x10u)
    out |= DDS_XTypes_IS_MUST_UNDERSTAND;
  if (flags & 0x20u)
    out |= DDS_XTypes_IS_KEY;
  if ((out & DDS_XTypes_IS_KEY) != 0)
    out &= static_cast<DDS_XTypes_StructMemberFlag>(~DDS_XTypes_IS_OPTIONAL);
  return out;
}

static DDS_XTypes_UnionMemberFlag valid_union_member_flags(uint32_t flags)
{
  DDS_XTypes_UnionMemberFlag out = valid_try_construct(flags);
  if (flags & 0x04u)
    out |= DDS_XTypes_IS_EXTERNAL;
  if (flags & 0x08u)
    out |= DDS_XTypes_IS_DEFAULT;
  return out;
}

static DDS_XTypes_CollectionElementFlag valid_collection_flags(uint32_t flags)
{
  DDS_XTypes_CollectionElementFlag out = valid_try_construct(flags);
  if (flags & 0x04u)
    out |= DDS_XTypes_IS_EXTERNAL;
  return out;
}

static DDS_XTypes_LBound valid_bound(uint32_t bound)
{
  return 1u + (bound % 16u);
}

static DDS_XTypes_BitBound valid_enum_bit_bound(uint32_t bit_bound)
{
  return static_cast<DDS_XTypes_BitBound>(1u + (bit_bound % 32u));
}

static DDS_XTypes_BitBound valid_bitmask_bit_bound(uint32_t bit_bound)
{
  return static_cast<DDS_XTypes_BitBound>(1u + (bit_bound % 64u));
}

static uint8_t primitive_kind(uint32_t flags)
{
  static const uint8_t primitives[] = {
    DDS_XTypes_TK_BOOLEAN,
    DDS_XTypes_TK_INT8,
    DDS_XTypes_TK_INT16,
    DDS_XTypes_TK_INT32,
    DDS_XTypes_TK_INT64,
    DDS_XTypes_TK_UINT8,
    DDS_XTypes_TK_UINT16,
    DDS_XTypes_TK_UINT32,
    DDS_XTypes_TK_UINT64,
    DDS_XTypes_TK_FLOAT32,
    DDS_XTypes_TK_FLOAT64,
    DDS_XTypes_TK_CHAR8,
    DDS_XTypes_TK_CHAR16
  };
  return primitives[flags % (sizeof(primitives) / sizeof(primitives[0]))];
}

static uint8_t normalized_kind(int proto_kind)
{
  switch (proto_kind)
  {
    case 1: return DDS_XTypes_TK_STRUCTURE;
    case 2: return DDS_XTypes_TK_UNION;
    case 3: return DDS_XTypes_TK_ALIAS;
    case 4: return DDS_XTypes_TK_ENUM;
    case 5: return DDS_XTypes_TK_BITMASK;
    case 6: return DDS_XTypes_TK_SEQUENCE;
    case 7: return DDS_XTypes_TK_ARRAY;
    case 8: return DDS_XTypes_TK_MAP;
    default: return DDS_XTypes_TK_INT32;
  }
}

static void set_hash_id(ddsi_type &type, uint32_t index)
{
  type.xt.kind = DDSI_TYPEID_KIND_COMPLETE;
  type.xt.id.x._d = DDS_XTypes_EK_COMPLETE;
  for (size_t n = 0; n < sizeof(type.xt.id.x._u.equivalence_hash); n++)
    type.xt.id.x._u.equivalence_hash[n] = static_cast<uint8_t>(index * 31u + n);
}

static void set_type_name(xt_type_detail &detail, const char *prefix, uint32_t index)
{
  char name[64];
  (void) snprintf(name, sizeof(name), "%s%u", prefix, index);
  ddsrt_strlcpy(detail.type_name, name, sizeof(detail.type_name));
}

static void set_member_name(xt_member_detail &detail, const char *prefix, uint32_t type_index, uint32_t member_index)
{
  char name[64];
  (void) snprintf(name, sizeof(name), "%s%u_%u", prefix, type_index, member_index);
  ddsrt_strlcpy(detail.name, name, sizeof(detail.name));
  ddsi_xt_get_namehash(detail.name_hash, detail.name);
}

class Runtime {
public:
  explicit Runtime(bool allow_recursive_types)
  {
    ddsi_iid_init();
    ddsi_thread_states_init();

    thrst_ = ddsi_lookup_thread_state();
    assert(thrst_->state == DDSI_THREAD_STATE_LAZILY_CREATED);
    thrst_->state = DDSI_THREAD_STATE_ALIVE;

    memset(&gv_, 0, sizeof(gv_));
    ddsrt_atomic_stvoidp(&thrst_->gv, &gv_);
    ddsi_config_init_default(&gv_.config);
    gv_.config.transport_selector = DDSI_TRANS_NONE;
    gv_.config.allow_recursive_types = allow_recursive_types;
    ddsi_config_prep(&gv_, g_cfgst);
    dds_set_log_sink(null_log_sink, nullptr);
    dds_set_trace_sink(null_log_sink, nullptr);

    ddsi_init(&gv_, nullptr);
  }

  ~Runtime()
  {
    ddsi_fini(&gv_);
    thrst_->state = DDSI_THREAD_STATE_LAZILY_CREATED;
    ddsi_thread_states_fini();
    ddsi_iid_fini();
  }

  ddsi_domaingv *gv()
  {
    return &gv_;
  }

private:
  ddsi_domaingv gv_;
  ddsi_thread_state *thrst_ = nullptr;
};

class TypeGraph {
public:
  TypeGraph(ddsi_domaingv *gv, const fuzz_type_graph::FuzzMsg &msg)
    : gv_(gv)
  {
    const uint32_t n_types = bounded_count(msg.types_size(), kMaxTypes);
    if (n_types == 0)
      return;

    init_builtin(int32_type_, DDS_XTypes_TK_INT32);
    init_builtin(uint32_type_, DDS_XTypes_TK_UINT32);
    types_.resize(n_types);
    for (uint32_t n = 0; n < n_types; n++)
    {
      memset(&types_[n].type, 0, sizeof(types_[n].type));
      types_[n].type.gv = gv_;
      types_[n].type.refc = 1;
      types_[n].type.state = DDSI_TYPE_RESOLVED;
      set_hash_id(types_[n].type, n + 1);
    }

    for (uint32_t n = 0; n < n_types; n++)
      build_type(n, msg.types(static_cast<int>(n)));

    root_ = &types_[bounded_index(msg.root(), n_types)].type;
  }

  ddsi_type *root() const
  {
    return root_;
  }

  std::vector<OwnedType> &types()
  {
    return types_;
  }

  ddsi_type *int32_type()
  {
    return &int32_type_;
  }

  dds_return_t validate() const
  {
    if (root_ == nullptr)
      return DDS_RETCODE_BAD_PARAMETER;
    return ddsi_xt_validate(gv_, root_);
  }

  void apply_graph_mutations(const fuzz_type_graph::FuzzMsg &msg)
  {
    const uint32_t n = bounded_count(msg.mutations_size(), kMaxMutations);
    for (uint32_t i = 0; i < n; i++)
    {
      const fuzz_type_graph::Mutation &mutation = msg.mutations(static_cast<int>(i));
      if (types_.empty())
        return;
      OwnedType &slot = types_[bounded_index(mutation.target(), static_cast<uint32_t>(types_.size()))];
      switch (static_cast<int>(mutation.kind()))
      {
        case 1:
          mutate_bad_type_flags(slot);
          break;
        case 2:
          mutate_bad_member_flags(slot);
          break;
        case 3:
          mutate_duplicate_member_id(slot);
          break;
        case 4:
          mutate_drop_indirection(slot);
          break;
        case 5:
          mutate_duplicate_enum_value(slot);
          break;
        case 6:
          mutate_bad_bitmask_position(slot);
          break;
        default:
          break;
      }
    }
  }

private:
  static void init_builtin(ddsi_type &type, uint8_t kind)
  {
    memset(&type, 0, sizeof(type));
    type.refc = 1;
    type.state = DDSI_TYPE_RESOLVED;
    type.xt._d = kind;
    type.xt.id.x._d = kind;
    type.xt.kind = DDSI_TYPEID_KIND_FULLY_DESCRIPTIVE;
  }

  ddsi_type *resolve_ref(uint32_t index)
  {
    if (types_.empty())
      return &int32_type_;
    return &types_[bounded_index(index, static_cast<uint32_t>(types_.size()))].type;
  }

  ddsi_type *resolve_acyclic_ref(uint32_t index, uint32_t owner_index)
  {
    if (owner_index == 0 || types_.empty())
      return &int32_type_;
    const uint32_t bounded = bounded_index(index, owner_index);
    return &types_[bounded].type;
  }

  std::unique_ptr<OwnedType> make_collection_wrapper(uint32_t wrap, ddsi_type *element, uint32_t flags, uint32_t bound, uint32_t index)
  {
    if (wrap == 0)
      return nullptr;
    std::unique_ptr<OwnedType> owned(new OwnedType());
    memset(&owned->type, 0, sizeof(owned->type));
    owned->type.gv = gv_;
    owned->type.refc = 1;
    owned->type.state = DDSI_TYPE_RESOLVED;
    owned->type.xt.kind = DDSI_TYPEID_KIND_PLAIN_COLLECTION_COMPLETE;
    owned->type.xt.id.x._d = DDS_XTypes_TK_NONE;
    if (wrap == 2)
    {
      owned->type.xt._d = DDS_XTypes_TK_ARRAY;
      owned->type.xt._u.array.c.ek = DDS_XTypes_EK_COMPLETE;
      owned->type.xt._u.array.c.element_type = element;
      owned->type.xt._u.array.c.element_flags = valid_collection_flags(flags);
      owned->bounds.push_back(valid_bound(bound));
      owned->type.xt._u.array.bounds._maximum = static_cast<uint32_t>(owned->bounds.size());
      owned->type.xt._u.array.bounds._length = static_cast<uint32_t>(owned->bounds.size());
      owned->type.xt._u.array.bounds._buffer = owned->bounds.data();
      owned->type.xt._u.array.bounds._release = false;
      set_type_name(owned->type.xt._u.array.c.detail, "FuzzArray", index);
    }
    else
    {
      owned->type.xt._d = DDS_XTypes_TK_SEQUENCE;
      owned->type.xt._u.seq.c.ek = DDS_XTypes_EK_COMPLETE;
      owned->type.xt._u.seq.c.element_type = element;
      owned->type.xt._u.seq.c.element_flags = valid_collection_flags(flags);
      owned->type.xt._u.seq.bound = valid_bound(bound);
      set_type_name(owned->type.xt._u.seq.c.detail, "FuzzSeq", index);
    }
    return owned;
  }

  ddsi_type *member_ref(const fuzz_type_graph::Member &member, uint32_t owner_index, uint32_t member_index)
  {
    const uint32_t wrap = static_cast<uint32_t>(member.wrap()) % 3u;
    ddsi_type *type = wrap == 0 ? resolve_acyclic_ref(member.type(), owner_index) : resolve_ref(member.type());
    std::unique_ptr<OwnedType> wrapper = make_collection_wrapper(wrap, type, member.flags(), member.id() + member_index, owner_index * kMaxMembers + member_index);
    if (wrapper == nullptr)
      return type;
    ddsi_type *wrapped = &wrapper->type;
    wrappers_.push_back(std::move(wrapper));
    return wrapped;
  }

  void build_type(uint32_t index, const fuzz_type_graph::Type &model)
  {
    OwnedType &slot = types_[index];
    ddsi_type &type = slot.type;
    uint8_t kind = normalized_kind(static_cast<int>(model.kind()));
    if (kind == DDS_XTypes_TK_INT32)
      kind = primitive_kind(model.flags());
    type.xt._d = kind;

    switch (kind)
    {
      case DDS_XTypes_TK_BOOLEAN:
      case DDS_XTypes_TK_INT8:
      case DDS_XTypes_TK_INT16:
      case DDS_XTypes_TK_INT32:
      case DDS_XTypes_TK_INT64:
      case DDS_XTypes_TK_UINT8:
      case DDS_XTypes_TK_UINT16:
      case DDS_XTypes_TK_UINT32:
      case DDS_XTypes_TK_UINT64:
      case DDS_XTypes_TK_FLOAT32:
      case DDS_XTypes_TK_FLOAT64:
      case DDS_XTypes_TK_CHAR8:
      case DDS_XTypes_TK_CHAR16:
        type.xt.id.x._d = kind;
        type.xt.kind = DDSI_TYPEID_KIND_FULLY_DESCRIPTIVE;
        break;
      case DDS_XTypes_TK_STRUCTURE:
        build_struct(index, model, slot);
        break;
      case DDS_XTypes_TK_UNION:
        build_union(index, model, slot);
        break;
      case DDS_XTypes_TK_ALIAS:
        type.xt._u.alias.flags = 0;
        type.xt._u.alias.related_flags = 0;
        type.xt._u.alias.related_type = resolve_acyclic_ref(model.related_type(), index);
        set_type_name(type.xt._u.alias.detail, "FuzzAlias", index);
        break;
      case DDS_XTypes_TK_ENUM:
        build_enum(index, model, slot);
        break;
      case DDS_XTypes_TK_BITMASK:
        build_bitmask(index, model, slot);
        break;
      case DDS_XTypes_TK_SEQUENCE:
        type.xt.kind = DDSI_TYPEID_KIND_PLAIN_COLLECTION_COMPLETE;
        type.xt._u.seq.c.ek = DDS_XTypes_EK_COMPLETE;
        type.xt._u.seq.c.element_type = resolve_acyclic_ref(model.related_type(), index);
        type.xt._u.seq.c.element_flags = valid_collection_flags(model.flags());
        type.xt._u.seq.bound = valid_bound(model.bound());
        set_type_name(type.xt._u.seq.c.detail, "FuzzSeq", index);
        break;
      case DDS_XTypes_TK_ARRAY:
        type.xt.kind = DDSI_TYPEID_KIND_PLAIN_COLLECTION_COMPLETE;
        type.xt._u.array.c.ek = DDS_XTypes_EK_COMPLETE;
        type.xt._u.array.c.element_type = resolve_acyclic_ref(model.related_type(), index);
        type.xt._u.array.c.element_flags = valid_collection_flags(model.flags());
        slot.bounds.push_back(valid_bound(model.bound()));
        type.xt._u.array.bounds._maximum = static_cast<uint32_t>(slot.bounds.size());
        type.xt._u.array.bounds._length = static_cast<uint32_t>(slot.bounds.size());
        type.xt._u.array.bounds._buffer = slot.bounds.data();
        type.xt._u.array.bounds._release = false;
        set_type_name(type.xt._u.array.c.detail, "FuzzArray", index);
        break;
      case DDS_XTypes_TK_MAP:
        type.xt.kind = DDSI_TYPEID_KIND_PLAIN_COLLECTION_COMPLETE;
        type.xt._u.map.c.ek = DDS_XTypes_EK_COMPLETE;
        type.xt._u.map.key_type = &uint32_type_;
        type.xt._u.map.c.element_type = resolve_acyclic_ref(model.related_type(), index);
        type.xt._u.map.key_flags = valid_collection_flags(model.flags());
        type.xt._u.map.c.element_flags = valid_collection_flags(model.flags() >> 3);
        type.xt._u.map.bound = valid_bound(model.bound());
        set_type_name(type.xt._u.map.c.detail, "FuzzMap", index);
        break;
      default:
        type.xt._d = DDS_XTypes_TK_INT32;
        type.xt.id.x._d = DDS_XTypes_TK_INT32;
        type.xt.kind = DDSI_TYPEID_KIND_FULLY_DESCRIPTIVE;
        break;
    }
  }

  void build_struct(uint32_t index, const fuzz_type_graph::Type &model, OwnedType &slot)
  {
    ddsi_type &type = slot.type;
    type.xt._u.structure.flags = valid_aggregate_flags(model.flags());
    set_type_name(type.xt._u.structure.detail, "FuzzStruct", index);
    const uint32_t n_members = bounded_count(model.members_size(), kMaxMembers);
    slot.struct_members.resize(n_members);
    for (uint32_t n = 0; n < n_members; n++)
    {
      const fuzz_type_graph::Member &src = model.members(static_cast<int>(n));
      xt_struct_member &dst = slot.struct_members[n];
      memset(&dst, 0, sizeof(dst));
      dst.id = src.id() % 0x0fffffffu;
      dst.flags = valid_struct_member_flags(src.flags());
      dst.type = member_ref(src, index, n);
      set_member_name(dst.detail, "m", index, n);
    }
    type.xt._u.structure.members.length = n_members;
    type.xt._u.structure.members.seq = slot.struct_members.data();
  }

  void build_union(uint32_t index, const fuzz_type_graph::Type &model, OwnedType &slot)
  {
    ddsi_type &type = slot.type;
    type.xt._u.union_type.flags = valid_aggregate_flags(model.flags());
    type.xt._u.union_type.disc_type = &int32_type_;
    type.xt._u.union_type.disc_flags = DDS_XTypes_TRY_CONSTRUCT1;
    set_type_name(type.xt._u.union_type.detail, "FuzzUnion", index);
    const uint32_t n_members = bounded_count(model.members_size(), kMaxMembers);
    slot.union_members.resize(n_members);
    slot.union_labels.resize(n_members);
    bool has_default = false;
    for (uint32_t n = 0; n < n_members; n++)
    {
      const fuzz_type_graph::Member &src = model.members(static_cast<int>(n));
      xt_union_member &dst = slot.union_members[n];
      memset(&dst, 0, sizeof(dst));
      dst.id = src.id() % 0x0fffffffu;
      dst.flags = valid_union_member_flags(src.flags());
      if ((dst.flags & DDS_XTypes_IS_DEFAULT) != 0)
      {
        if (has_default)
          dst.flags &= static_cast<DDS_XTypes_UnionMemberFlag>(~DDS_XTypes_IS_DEFAULT);
        has_default = true;
      }
      dst.type = member_ref(src, index, n);
      const uint32_t n_labels = std::max<uint32_t>(1, bounded_count(src.labels_size(), kMaxValues));
      slot.union_labels[n].resize(n_labels);
      for (uint32_t i = 0; i < n_labels; i++)
        slot.union_labels[n][i] = i < static_cast<uint32_t>(src.labels_size()) ? src.labels(static_cast<int>(i)).value() : static_cast<int32_t>(n);
      dst.label_seq._maximum = n_labels;
      dst.label_seq._length = n_labels;
      dst.label_seq._buffer = slot.union_labels[n].data();
      dst.label_seq._release = false;
      set_member_name(dst.detail, "u", index, n);
    }
    type.xt._u.union_type.members.length = n_members;
    type.xt._u.union_type.members.seq = slot.union_members.data();
  }

  void build_enum(uint32_t index, const fuzz_type_graph::Type &model, OwnedType &slot)
  {
    ddsi_type &type = slot.type;
    type.xt._u.enum_type.flags = valid_enum_flags(model.flags());
    type.xt._u.enum_type.bit_bound = valid_enum_bit_bound(model.bit_bound());
    set_type_name(type.xt._u.enum_type.detail, "FuzzEnum", index);
    const uint32_t n_literals = std::max<uint32_t>(1, bounded_count(model.values_size(), kMaxValues));
    slot.enum_literals.resize(n_literals);
    for (uint32_t n = 0; n < n_literals; n++)
    {
      xt_enum_literal &literal = slot.enum_literals[n];
      memset(&literal, 0, sizeof(literal));
      literal.value = n < static_cast<uint32_t>(model.values_size()) ? model.values(static_cast<int>(n)).value() : static_cast<int32_t>(n);
      literal.flags = 0;
      set_member_name(literal.detail, "e", index, n);
    }
    type.xt._u.enum_type.literals.length = n_literals;
    type.xt._u.enum_type.literals.seq = slot.enum_literals.data();
  }

  void build_bitmask(uint32_t index, const fuzz_type_graph::Type &model, OwnedType &slot)
  {
    ddsi_type &type = slot.type;
    type.xt._u.bitmask.flags = valid_enum_flags(model.flags());
    type.xt._u.bitmask.bit_bound = valid_bitmask_bit_bound(model.bit_bound());
    set_type_name(type.xt._u.bitmask.detail, "FuzzBitmask", index);
    const uint32_t n_flags = std::max<uint32_t>(1, bounded_count(model.values_size(), kMaxValues));
    slot.bitflags.resize(n_flags);
    for (uint32_t n = 0; n < n_flags; n++)
    {
      xt_bitflag &flag = slot.bitflags[n];
      memset(&flag, 0, sizeof(flag));
      uint32_t position = n < static_cast<uint32_t>(model.values_size()) ? static_cast<uint32_t>(model.values(static_cast<int>(n)).value()) : n;
      flag.position = static_cast<uint16_t>(position % type.xt._u.bitmask.bit_bound);
      flag.flags = 0;
      set_member_name(flag.detail, "b", index, n);
    }
    type.xt._u.bitmask.bitflags.length = n_flags;
    type.xt._u.bitmask.bitflags.seq = slot.bitflags.data();
  }

  static void mutate_bad_type_flags(OwnedType &slot)
  {
    switch (slot.type.xt._d)
    {
      case DDS_XTypes_TK_STRUCTURE:
        slot.type.xt._u.structure.flags |= 0x8000u;
        break;
      case DDS_XTypes_TK_UNION:
        slot.type.xt._u.union_type.flags |= 0x8000u;
        break;
      case DDS_XTypes_TK_ENUM:
        slot.type.xt._u.enum_type.flags |= 0x8000u;
        break;
      case DDS_XTypes_TK_BITMASK:
        slot.type.xt._u.bitmask.flags |= 0x8000u;
        break;
      case DDS_XTypes_TK_ALIAS:
        slot.type.xt._u.alias.flags |= 0x8000u;
        break;
      case DDS_XTypes_TK_SEQUENCE:
        slot.type.xt._u.seq.c.flags |= 0x8000u;
        break;
      case DDS_XTypes_TK_ARRAY:
        slot.type.xt._u.array.c.flags |= 0x8000u;
        break;
      case DDS_XTypes_TK_MAP:
        slot.type.xt._u.map.c.flags |= 0x8000u;
        break;
      default:
        break;
    }
  }

  static void mutate_bad_member_flags(OwnedType &slot)
  {
    if (slot.type.xt._d == DDS_XTypes_TK_STRUCTURE && slot.type.xt._u.structure.members.length > 0)
      slot.type.xt._u.structure.members.seq[0].flags |= 0x8000u;
    else if (slot.type.xt._d == DDS_XTypes_TK_UNION && slot.type.xt._u.union_type.members.length > 0)
      slot.type.xt._u.union_type.members.seq[0].flags |= 0x8000u;
    else if (slot.type.xt._d == DDS_XTypes_TK_SEQUENCE)
      slot.type.xt._u.seq.c.element_flags |= 0x8000u;
    else if (slot.type.xt._d == DDS_XTypes_TK_ARRAY)
      slot.type.xt._u.array.c.element_flags |= 0x8000u;
    else if (slot.type.xt._d == DDS_XTypes_TK_MAP)
      slot.type.xt._u.map.c.element_flags |= 0x8000u;
  }

  static void mutate_duplicate_member_id(OwnedType &slot)
  {
    if (slot.type.xt._d == DDS_XTypes_TK_STRUCTURE && slot.type.xt._u.structure.members.length > 1)
      slot.type.xt._u.structure.members.seq[1].id = slot.type.xt._u.structure.members.seq[0].id;
    else if (slot.type.xt._d == DDS_XTypes_TK_UNION && slot.type.xt._u.union_type.members.length > 1)
      slot.type.xt._u.union_type.members.seq[1].id = slot.type.xt._u.union_type.members.seq[0].id;
  }

  static void mutate_drop_indirection(OwnedType &slot)
  {
    if (slot.type.xt._d != DDS_XTypes_TK_STRUCTURE || slot.type.xt._u.structure.members.length == 0)
      return;
    xt_struct_member &member = slot.type.xt._u.structure.members.seq[0];
    member.flags &= static_cast<DDS_XTypes_StructMemberFlag>(~(DDS_XTypes_IS_EXTERNAL | DDS_XTypes_IS_OPTIONAL));
    if (member.type && member.type->xt._d == DDS_XTypes_TK_SEQUENCE)
      member.type = member.type->xt._u.seq.c.element_type;
    else if (member.type && member.type->xt._d == DDS_XTypes_TK_ARRAY)
      member.type = member.type->xt._u.array.c.element_type;
  }

  static void mutate_duplicate_enum_value(OwnedType &slot)
  {
    if (slot.type.xt._d == DDS_XTypes_TK_ENUM && slot.type.xt._u.enum_type.literals.length > 1)
      slot.type.xt._u.enum_type.literals.seq[1].value = slot.type.xt._u.enum_type.literals.seq[0].value;
  }

  static void mutate_bad_bitmask_position(OwnedType &slot)
  {
    if (slot.type.xt._d == DDS_XTypes_TK_BITMASK && slot.type.xt._u.bitmask.bitflags.length > 0)
      slot.type.xt._u.bitmask.bitflags.seq[0].position = slot.type.xt._u.bitmask.bit_bound;
  }

  ddsi_domaingv *gv_;
  std::vector<OwnedType> types_;
  std::vector<std::unique_ptr<OwnedType>> wrappers_;
  ddsi_type int32_type_;
  ddsi_type uint32_type_;
  ddsi_type *root_ = nullptr;
};

static void mutate_typeobject(DDS_XTypes_TypeObject &type_object)
{
  if (type_object._d == DDS_XTypes_EK_COMPLETE)
  {
    switch (type_object._u.complete._d)
    {
      case DDS_XTypes_TK_STRUCTURE:
        type_object._u.complete._u.struct_type.header.detail.type_name[0] ^= 1;
        break;
      case DDS_XTypes_TK_UNION:
        type_object._u.complete._u.union_type.header.detail.type_name[0] ^= 1;
        break;
      case DDS_XTypes_TK_ALIAS:
        type_object._u.complete._u.alias_type.header.detail.type_name[0] ^= 1;
        break;
      case DDS_XTypes_TK_ENUM:
        if (type_object._u.complete._u.enumerated_type.literal_seq._length > 0)
          type_object._u.complete._u.enumerated_type.literal_seq._buffer[0].common.value ^= 1;
        break;
      case DDS_XTypes_TK_BITMASK:
        type_object._u.complete._u.bitmask_type.header.common.bit_bound ^= 1;
        break;
      default:
        break;
    }
  }
  else if (type_object._d == DDS_XTypes_EK_MINIMAL)
  {
    switch (type_object._u.minimal._d)
    {
      case DDS_XTypes_TK_STRUCTURE:
        if (type_object._u.minimal._u.struct_type.member_seq._length > 0)
          type_object._u.minimal._u.struct_type.member_seq._buffer[0].detail.name_hash[0] ^= 1;
        break;
      case DDS_XTypes_TK_UNION:
        if (type_object._u.minimal._u.union_type.member_seq._length > 0)
          type_object._u.minimal._u.union_type.member_seq._buffer[0].detail.name_hash[0] ^= 1;
        break;
      case DDS_XTypes_TK_ENUM:
        if (type_object._u.minimal._u.enumerated_type.literal_seq._length > 0)
          type_object._u.minimal._u.enumerated_type.literal_seq._buffer[0].common.value ^= 1;
        break;
      case DDS_XTypes_TK_BITMASK:
        type_object._u.minimal._u.bitmask_type.header.common.bit_bound ^= 1;
        break;
      default:
        break;
    }
  }
}

static void apply_typeobject_mutations(DDS_XTypes_TypeObject &type_object, const fuzz_type_graph::FuzzMsg &msg)
{
  const uint32_t n = bounded_count(msg.mutations_size(), kMaxMutations);
  for (uint32_t i = 0; i < n; i++)
  {
    if (static_cast<int>(msg.mutations(static_cast<int>(i)).kind()) == 7)
      mutate_typeobject(type_object);
  }
}

static bool same_scc_component(const DDS_XTypes_TypeIdentifier &a, const DDS_XTypes_TypeIdentifier &b)
{
  return a._d == DDS_XTypes_TI_STRONGLY_CONNECTED_COMPONENT &&
    b._d == DDS_XTypes_TI_STRONGLY_CONNECTED_COMPONENT &&
    ddsi_type_scc_id_same_component_impl(&a._u.sc_component_id, &b._u.sc_component_id);
}

static SccPairSet make_scc_pairs(TypeGraph &graph, DDS_XTypes_TypeIdentifier &root_id, ddsi_typeid_kind_t kind)
{
  SccPairSet out;
  out.pairs.reserve(graph.types().size());
  for (OwnedType &slot : graph.types())
  {
    if (!is_hash_type(slot.type.xt._d))
      continue;
    DDS_XTypes_TypeIdentifier type_id;
    memset(&type_id, 0, sizeof(type_id));
    ddsi_xt_get_typeid_impl(&slot.type.xt, &type_id, kind);
    if (!same_scc_component(type_id, root_id))
    {
      ddsi_typeid_fini_impl(&type_id);
      continue;
    }
    DDS_XTypes_TypeIdentifierTypeObjectPair pair;
    memset(&pair, 0, sizeof(pair));
    pair.type_identifier = type_id;
    ddsi_xt_get_typeobject_kind_impl(&slot.type.xt, &pair.type_object, kind);
    out.pairs.push_back(pair);
  }
  out.seq_length = static_cast<uint32_t>(out.pairs.size());
  return out;
}

static void apply_scc_mutations(SccPairSet &pairs, const fuzz_type_graph::FuzzMsg &msg)
{
  if (pairs.pairs.empty())
    return;
  const uint32_t n = bounded_count(msg.mutations_size(), kMaxMutations);
  for (uint32_t i = 0; i < n; i++)
  {
    const fuzz_type_graph::Mutation &mutation = msg.mutations(static_cast<int>(i));
    DDS_XTypes_TypeIdentifierTypeObjectPair &target =
      pairs.pairs[bounded_index(mutation.target(), static_cast<uint32_t>(pairs.pairs.size()))];
    switch (static_cast<int>(mutation.kind()))
    {
      case 7:
        mutate_typeobject(target.type_object);
        break;
      case 8:
        if (target.type_identifier._d == DDS_XTypes_TI_STRONGLY_CONNECTED_COMPONENT)
          target.type_identifier._u.sc_component_id.scc_index =
            target.type_identifier._u.sc_component_id.scc_length + 1;
        break;
      case 9:
        if (target.type_identifier._d == DDS_XTypes_TI_STRONGLY_CONNECTED_COMPONENT)
          target.type_identifier._u.sc_component_id.scc_length =
            target.type_identifier._u.sc_component_id.scc_length + 1;
        break;
      case 10:
        if (target.type_identifier._d == DDS_XTypes_TI_STRONGLY_CONNECTED_COMPONENT)
          target.type_identifier._u.sc_component_id.sc_component_id._u.hash[0] ^= 0x80;
        break;
      case 11:
        if (!pairs.pairs.empty())
        {
          DDS_XTypes_TypeIdentifierTypeObjectPair duplicate;
          memset(&duplicate, 0, sizeof(duplicate));
          duplicate.type_identifier = pairs.pairs[0].type_identifier;
          duplicate.type_object._d = pairs.pairs[0].type_object._d;
          pairs.pairs.push_back(duplicate);
          pairs.seq_length = static_cast<uint32_t>(pairs.pairs.size());
        }
        break;
      case 12:
        if (pairs.seq_length > 0)
          pairs.seq_length--;
        break;
      case 13:
        if (target.type_identifier._d == DDS_XTypes_TI_STRONGLY_CONNECTED_COMPONENT)
          target.type_identifier._u.sc_component_id.sc_component_id._d =
            target.type_identifier._u.sc_component_id.sc_component_id._d == DDS_XTypes_EK_COMPLETE ?
            DDS_XTypes_EK_MINIMAL : DDS_XTypes_EK_COMPLETE;
        break;
      default:
        break;
    }
  }
}

static void fini_scc_pairs(SccPairSet &pairs)
{
  for (DDS_XTypes_TypeIdentifierTypeObjectPair &pair : pairs.pairs)
  {
    ddsi_typeid_fini_impl(&pair.type_identifier);
    ddsi_typeobj_fini_impl(&pair.type_object);
  }
}

static void replace_typeid(DDS_XTypes_TypeIdentifier &dst, const DDS_XTypes_TypeIdentifier &src)
{
  if (&dst == &src)
    return;
  ddsi_typeid_fini_impl(&dst);
  memset(&dst, 0, sizeof(dst));
  ddsi_typeid_copy_impl(&dst, &src);
}

static void set_typeid_none(DDS_XTypes_TypeIdentifier &type_id)
{
  ddsi_typeid_fini_impl(&type_id);
  memset(&type_id, 0, sizeof(type_id));
  type_id._d = DDS_XTypes_TK_NONE;
}

static void mutate_scc_typeid(DDS_XTypes_TypeIdentifier &type_id, int mutation_kind)
{
  if (type_id._d != DDS_XTypes_TI_STRONGLY_CONNECTED_COMPONENT)
    return;

  switch (mutation_kind)
  {
    case fuzz_type_graph::MUTATION_SCC_BAD_INDEX:
      type_id._u.sc_component_id.scc_index = type_id._u.sc_component_id.scc_length + 1;
      break;
    case fuzz_type_graph::MUTATION_SCC_BAD_LENGTH:
      type_id._u.sc_component_id.scc_length = type_id._u.sc_component_id.scc_length + 1;
      break;
    case fuzz_type_graph::MUTATION_SCC_BAD_HASH:
      type_id._u.sc_component_id.sc_component_id._u.hash[0] ^= 0x80;
      break;
    case fuzz_type_graph::MUTATION_SCC_WRONG_EQUIV_KIND:
      type_id._u.sc_component_id.sc_component_id._d =
        type_id._u.sc_component_id.sc_component_id._d == DDS_XTypes_EK_COMPLETE ?
        DDS_XTypes_EK_MINIMAL : DDS_XTypes_EK_COMPLETE;
      break;
    default:
      break;
  }
}

static DDS_XTypes_TypeIdentifier *select_typeinfo_typeid(
  DDS_XTypes_TypeIdentifierWithDependencies &typeinfo,
  uint32_t target)
{
  const uint32_t n_typeids = typeinfo.dependent_typeids._length + 1;
  const uint32_t idx = bounded_index(target, n_typeids);
  if (idx == 0)
    return &typeinfo.typeid_with_size.type_id;
  return &typeinfo.dependent_typeids._buffer[idx - 1].type_id;
}

static DDS_XTypes_TypeIdentifier *select_typeinfo_typeid(ddsi_typeinfo_t *type_info, uint32_t target)
{
  if (type_info == nullptr)
    return nullptr;
  return (target & 1u) == 0 ?
    select_typeinfo_typeid(type_info->x.complete, target >> 1) :
    select_typeinfo_typeid(type_info->x.minimal, target >> 1);
}

static DDS_XTypes_TypeIdentifierTypeObjectPair *select_typemap_typeobj_pair(ddsi_typemap_t *type_map, uint32_t target)
{
  if (type_map == nullptr)
    return nullptr;
  dds_sequence_DDS_XTypes_TypeIdentifierTypeObjectPair *seqs[] = {
    &type_map->x.identifier_object_pair_complete,
    &type_map->x.identifier_object_pair_minimal
  };
  const uint32_t total = seqs[0]->_length + seqs[1]->_length;
  if (total == 0)
    return nullptr;

  uint32_t idx = bounded_index(target, total);
  for (dds_sequence_DDS_XTypes_TypeIdentifierTypeObjectPair *seq : seqs)
  {
    if (idx < seq->_length)
      return &seq->_buffer[idx];
    idx -= seq->_length;
  }
  return nullptr;
}

static DDS_XTypes_TypeIdentifier *select_typemap_pair_typeid(ddsi_typemap_t *type_map, uint32_t target)
{
  if (type_map == nullptr || type_map->x.identifier_complete_minimal._length == 0)
    return nullptr;

  dds_sequence_DDS_XTypes_TypeIdentifierPair &seq = type_map->x.identifier_complete_minimal;
  DDS_XTypes_TypeIdentifierPair &pair =
    seq._buffer[bounded_index(target >> 1, seq._length)];
  return (target & 1u) == 0 ? &pair.type_identifier1 : &pair.type_identifier2;
}

static void duplicate_typemap_typeid(ddsi_typemap_t *type_map, uint32_t target)
{
  if (type_map == nullptr)
    return;

  dds_sequence_DDS_XTypes_TypeIdentifierTypeObjectPair &complete = type_map->x.identifier_object_pair_complete;
  if (complete._length > 1)
  {
    const uint32_t dst = 1u + bounded_index(target, complete._length - 1u);
    replace_typeid(complete._buffer[dst].type_identifier, complete._buffer[0].type_identifier);
    return;
  }

  dds_sequence_DDS_XTypes_TypeIdentifierTypeObjectPair &minimal = type_map->x.identifier_object_pair_minimal;
  if (minimal._length > 1)
  {
    const uint32_t dst = 1u + bounded_index(target, minimal._length - 1u);
    replace_typeid(minimal._buffer[dst].type_identifier, minimal._buffer[0].type_identifier);
  }
}

static void duplicate_typeinfo_typeid(ddsi_typeinfo_t *type_info, uint32_t target)
{
  if (type_info == nullptr)
    return;

  DDS_XTypes_TypeIdentifierWithDependencies &half =
    (target & 1u) == 0 ? type_info->x.complete : type_info->x.minimal;
  if (half.dependent_typeids._length == 0)
    return;
  DDS_XTypes_TypeIdentifier &dst =
    half.dependent_typeids._buffer[bounded_index(target >> 1, half.dependent_typeids._length)].type_id;
  replace_typeid(dst, half.typeid_with_size.type_id);
}

static void invalidate_typeinfo_dependent_count(ddsi_typeinfo_t *type_info, uint32_t target)
{
  if (type_info == nullptr)
    return;
  DDS_XTypes_TypeIdentifierWithDependencies &half =
    (target & 1u) == 0 ? type_info->x.complete : type_info->x.minimal;
  half.dependent_typeid_count =
    half.dependent_typeids._length > 0 ? static_cast<int32_t>(half.dependent_typeids._length - 1u) : -2;
}

static void apply_typeinfo_typemap_mutations(ddsi_typeinfo_t *type_info, ddsi_typemap_t *type_map, const fuzz_type_graph::FuzzMsg &msg)
{
  const uint32_t n = bounded_count(msg.mutations_size(), kMaxMutations);
  for (uint32_t i = 0; i < n; i++)
  {
    const fuzz_type_graph::Mutation &mutation = msg.mutations(static_cast<int>(i));
    const int kind = static_cast<int>(mutation.kind());
    const uint32_t target = mutation.target();

    DDS_XTypes_TypeIdentifierTypeObjectPair *typeobj_pair = select_typemap_typeobj_pair(type_map, target);
    DDS_XTypes_TypeIdentifier *mapping_typeid = select_typemap_pair_typeid(type_map, target);
    DDS_XTypes_TypeIdentifier *typeinfo_typeid = select_typeinfo_typeid(type_info, target);

    switch (kind)
    {
      case fuzz_type_graph::MUTATION_STALE_TYPEOBJECT_HASH:
        if (typeobj_pair != nullptr)
          mutate_typeobject(typeobj_pair->type_object);
        break;
      case fuzz_type_graph::MUTATION_SCC_BAD_INDEX:
      case fuzz_type_graph::MUTATION_SCC_BAD_LENGTH:
      case fuzz_type_graph::MUTATION_SCC_BAD_HASH:
      case fuzz_type_graph::MUTATION_SCC_WRONG_EQUIV_KIND:
        if (typeobj_pair != nullptr)
          mutate_scc_typeid(typeobj_pair->type_identifier, kind);
        if (mapping_typeid != nullptr)
          mutate_scc_typeid(*mapping_typeid, kind);
        if (typeinfo_typeid != nullptr)
          mutate_scc_typeid(*typeinfo_typeid, kind);
        break;
      case fuzz_type_graph::MUTATION_SCC_DUPLICATE_SLOT:
        duplicate_typemap_typeid(type_map, target);
        duplicate_typeinfo_typeid(type_info, target);
        break;
      case fuzz_type_graph::MUTATION_SCC_DROP_SLOT:
        if (typeobj_pair != nullptr)
          set_typeid_none(typeobj_pair->type_identifier);
        invalidate_typeinfo_dependent_count(type_info, target);
        break;
      default:
        break;
    }
  }
}

static void import_typeobject(
  Runtime &runtime,
  TypeGraph &graph,
  ddsi_type *root,
  ddsi_typeid_kind_t kind,
  const fuzz_type_graph::FuzzMsg &msg,
  bool mutate)
{
  if (root == nullptr || !is_hash_type(root->xt._d))
    return;

  DDS_XTypes_TypeIdentifier type_id;
  memset(&type_id, 0, sizeof(type_id));
  ddsi_xt_get_typeid_impl(&root->xt, &type_id, kind);

  if (type_id._d == DDS_XTypes_TI_STRONGLY_CONNECTED_COMPONENT)
  {
    SccPairSet pairs = make_scc_pairs(graph, type_id, kind);
    if (mutate)
      apply_scc_mutations(pairs, msg);
    if (!pairs.pairs.empty() && pairs.seq_length > 0)
    {
      dds_sequence_DDS_XTypes_TypeIdentifierTypeObjectPair pairseq;
      memset(&pairseq, 0, sizeof(pairseq));
      pairseq._maximum = static_cast<uint32_t>(pairs.pairs.size());
      pairseq._length = std::min<uint32_t>(pairs.seq_length, static_cast<uint32_t>(pairs.pairs.size()));
      pairseq._buffer = pairs.pairs.data();
      pairseq._release = false;

      bool complete = false;
      std::vector<ddsi_type *> refs;
      ddsrt_mutex_lock(&runtime.gv()->typelib_lock);
      for (uint32_t n = 0; n < pairseq._length; n++)
      {
        ddsi_type *type = nullptr;
        dds_return_t ret = ddsi_type_ref_id_locked_impl(runtime.gv(), &type, &pairseq._buffer[n].type_identifier);
        if (ret == DDS_RETCODE_OK && type != nullptr)
          refs.push_back(type);
      }
      if (!refs.empty())
        (void) ddsi_type_add_scc_typeobjs_locked(runtime.gv(), &pairseq, &pairs.pairs[0].type_identifier, true, &complete);
      for (ddsi_type *type : refs)
        ddsi_type_unref_locked(runtime.gv(), type);
      ddsrt_mutex_unlock(&runtime.gv()->typelib_lock);
    }
    fini_scc_pairs(pairs);
  }
  else
  {
    DDS_XTypes_TypeObject type_object;
    memset(&type_object, 0, sizeof(type_object));
    ddsi_xt_get_typeobject_kind_impl(&root->xt, &type_object, kind);
    if (mutate)
      apply_typeobject_mutations(type_object, msg);

    ddsi_type *type = nullptr;
    ddsrt_mutex_lock(&runtime.gv()->typelib_lock);
    dds_return_t ret = ddsi_type_ref_id_locked_impl(runtime.gv(), &type, &type_id);
    if (ret == DDS_RETCODE_OK && type != nullptr)
    {
      (void) ddsi_type_add_typeobj(runtime.gv(), type, &type_object);
      ddsi_type_unref_locked(runtime.gv(), type);
    }
    ddsrt_mutex_unlock(&runtime.gv()->typelib_lock);
    ddsi_typeobj_fini_impl(&type_object);
  }
  ddsi_typeid_fini_impl(&type_id);
}

static void import_typeobject(Runtime &runtime, TypeGraph &graph, ddsi_typeid_kind_t kind, const fuzz_type_graph::FuzzMsg &msg)
{
  import_typeobject(runtime, graph, graph.root(), kind, msg, true);
}

static void import_all_typeobjects(Runtime &runtime, TypeGraph &graph, ddsi_typeid_kind_t kind, const fuzz_type_graph::FuzzMsg &msg)
{
  for (OwnedType &slot : graph.types())
    import_typeobject(runtime, graph, &slot.type, kind, msg, false);
}

static void fini_serialized_typeinfo_typemap(SerializedTypeInfoTypeMap &serialized)
{
  ddsrt_free(serialized.typeinfo);
  ddsrt_free(serialized.typemap);
  serialized.typeinfo = nullptr;
  serialized.typemap = nullptr;
  serialized.typeinfo_size = 0;
  serialized.typemap_size = 0;
}

static bool can_export_typeinfo(const ddsi_type *type)
{
  return type != nullptr && type->xt.kind == DDSI_TYPEID_KIND_COMPLETE && is_hash_type(type->xt._d);
}

static bool export_typeinfo_typemap(const fuzz_type_graph::FuzzMsg &msg, SerializedTypeInfoTypeMap &serialized)
{
  Runtime source(msg.allow_recursive_types());
  TypeGraph graph(source.gv(), msg);
  if (!can_export_typeinfo(graph.root()) || graph.validate() != DDS_RETCODE_OK)
    return false;

  import_all_typeobjects(source, graph, DDSI_TYPEID_KIND_COMPLETE, msg);

  DDS_XTypes_TypeIdentifier root_id;
  memset(&root_id, 0, sizeof(root_id));
  ddsi_xt_get_typeid_impl(&graph.root()->xt, &root_id, DDSI_TYPEID_KIND_COMPLETE);

  ddsi_type *root = nullptr;
  ddsrt_mutex_lock(&source.gv()->typelib_lock);
  dds_return_t ret = ddsi_type_ref_id_locked_impl(source.gv(), &root, &root_id);
  ddsrt_mutex_unlock(&source.gv()->typelib_lock);
  ddsi_typeid_fini_impl(&root_id);

  if (ret != DDS_RETCODE_OK || root == nullptr)
    return false;

  if (!ddsi_type_resolved(source.gv(), root, DDSI_TYPE_INCLUDE_DEPS))
  {
    ddsi_type_unref(source.gv(), root);
    return false;
  }

  ret = ddsi_type_get_typeinfo_ser(source.gv(), root, &serialized.typeinfo, &serialized.typeinfo_size);
  if (ret == DDS_RETCODE_OK)
    ret = ddsi_type_get_typemap_ser(source.gv(), root, &serialized.typemap, &serialized.typemap_size);
  ddsi_type_unref(source.gv(), root);

  if (ret != DDS_RETCODE_OK)
  {
    fini_serialized_typeinfo_typemap(serialized);
    return false;
  }
  return true;
}

static void roundtrip_typeinfo_typemap(const fuzz_type_graph::FuzzMsg &msg)
{
  SerializedTypeInfoTypeMap serialized;
  if (!export_typeinfo_typemap(msg, serialized))
    return;

  ddsi_typeinfo_t *type_info = ddsi_typeinfo_deser(serialized.typeinfo, serialized.typeinfo_size);
  ddsi_typemap_t *type_map = ddsi_typemap_deser(serialized.typemap, serialized.typemap_size);
  fini_serialized_typeinfo_typemap(serialized);

  apply_typeinfo_typemap_mutations(type_info, type_map, msg);

  if (type_info != nullptr && type_map != nullptr)
  {
    Runtime sink(msg.allow_recursive_types());
    ddsi_type *type_minimal = nullptr;
    ddsi_type *type_complete = nullptr;
    (void) ddsi_type_add(sink.gv(), &type_minimal, &type_complete, type_info, type_map);
    if (type_minimal != nullptr)
      ddsi_type_unref(sink.gv(), type_minimal);
    if (type_complete != nullptr)
      ddsi_type_unref(sink.gv(), type_complete);
  }

  if (type_info != nullptr)
    ddsi_typeinfo_free(type_info);
  if (type_map != nullptr)
  {
    ddsi_typemap_fini(type_map);
    ddsrt_free(type_map);
  }
}

static void run_action(Runtime &runtime, TypeGraph &graph, int action, const fuzz_type_graph::FuzzMsg &msg)
{
  switch (action)
  {
    case 0:
    {
      TypeGraph validation_graph(runtime.gv(), msg);
      validation_graph.apply_graph_mutations(msg);
      (void) validation_graph.validate();
      break;
    }
    case 1:
      if (graph.validate() == DDS_RETCODE_OK)
        import_typeobject(runtime, graph, DDSI_TYPEID_KIND_COMPLETE, msg);
      break;
    case 2:
      if (graph.validate() == DDS_RETCODE_OK)
        import_typeobject(runtime, graph, DDSI_TYPEID_KIND_MINIMAL, msg);
      break;
    default:
      break;
  }
}

} // namespace

DEFINE_PROTO_FUZZER(const fuzz_type_graph::FuzzMsg &message)
{
  const uint32_t n_actions = bounded_count(message.actions_size(), kMaxActions);
  if (n_actions == 0)
  {
    roundtrip_typeinfo_typemap(message);

    Runtime runtime(message.allow_recursive_types());
    TypeGraph graph(runtime.gv(), message);
    if (graph.root() == nullptr)
      return;

    run_action(runtime, graph, 0, message);
    run_action(runtime, graph, 1, message);
    return;
  }

  bool run_regular_actions = false;
  for (uint32_t n = 0; n < n_actions; n++)
  {
    const int action = static_cast<int>(message.actions(static_cast<int>(n)).action());
    if (action == kActionRoundtripTypeInfoTypeMap)
      roundtrip_typeinfo_typemap(message);
    else
      run_regular_actions = true;
  }
  if (!run_regular_actions)
    return;

  Runtime runtime(message.allow_recursive_types());
  TypeGraph graph(runtime.gv(), message);
  if (graph.root() == nullptr)
    return;

  for (uint32_t n = 0; n < n_actions; n++)
  {
    const int action = static_cast<int>(message.actions(static_cast<int>(n)).action());
    if (action != kActionRoundtripTypeInfoTypeMap)
      run_action(runtime, graph, action, message);
  }
}
