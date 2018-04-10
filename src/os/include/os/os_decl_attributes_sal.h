/*
 * Copyright(c) 2006 to 2018 ADLINK Technology Limited and others
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */
#ifndef OS_DECL_ATTRIBUTES_SAL_H
#define OS_DECL_ATTRIBUTES_SAL_H

#if defined(_MSC_VER)
# include <sal.h>
  /* SAL-like extension to signify that the following line isn't reached */
# define _Noreturn_ __declspec(noreturn) _Analysis_noreturn_
#endif

#ifndef _Acquires_exclusive_lock_
#define _Acquires_exclusive_lock_(lock)
#endif
#ifndef _Acquires_lock_
#define _Acquires_lock_(lock)
#endif
#ifndef _Acquires_nonreentrant_lock_
#define _Acquires_nonreentrant_lock_(lock)
#endif
#ifndef _Acquires_shared_lock_
#define _Acquires_shared_lock_(lock)
#endif
#ifndef _Always_
#define _Always_(annos)
#endif
#ifndef _Analysis_assume_
#define _Analysis_assume_(expr)
#endif
#ifndef _Analysis_assume_
#define _Analysis_assume_(expr)
#endif
#ifndef _Analysis_assume_lock_acquired_
#define _Analysis_assume_lock_acquired_(lock)
#endif
#ifndef _Analysis_assume_lock_held_
#define _Analysis_assume_lock_held_(lock)
#endif
#ifndef _Analysis_assume_lock_not_held_
#define _Analysis_assume_lock_not_held_(lock)
#endif
#ifndef _Analysis_assume_lock_released_
#define _Analysis_assume_lock_released_(lock)
#endif
#ifndef _Analysis_assume_nullterminated_
#define _Analysis_assume_nullterminated_(x)
#endif
#ifndef _Analysis_assume_nullterminated_
#define _Analysis_assume_nullterminated_(x)
#endif
#ifndef _Analysis_assume_same_lock_
#define _Analysis_assume_same_lock_(lock1, lock2)
#endif
#ifndef _Analysis_mode_
#define _Analysis_mode_(mode)
#endif
#ifndef _Analysis_mode_
#define _Analysis_mode_(mode)
#endif
#ifndef _Analysis_noreturn_
#define _Analysis_noreturn_ __attribute_noreturn__
#endif
#ifndef _Analysis_suppress_lock_checking_
#define _Analysis_suppress_lock_checking_(lock)
#endif
#ifndef _At_
#define _At_(target, annos)
#endif
#ifndef _At_buffer_
#define _At_buffer_(target, iter, bound, annos)
#endif
#ifndef _Benign_race_begin_
#define _Benign_race_begin_
#endif
#ifndef _Benign_race_end_
#define _Benign_race_end_
#endif
#ifndef _COM_Outptr_
#define _COM_Outptr_
#endif
#ifndef _COM_Outptr_opt_
#define _COM_Outptr_opt_
#endif
#ifndef _COM_Outptr_opt_result_maybenull_
#define _COM_Outptr_opt_result_maybenull_
#endif
#ifndef _COM_Outptr_result_maybenull_
#define _COM_Outptr_result_maybenull_
#endif
#ifndef _Called_from_function_class_
#define _Called_from_function_class_(x)
#endif
#ifndef _Check_return_
#define _Check_return_ __attribute_warn_unused_result__
#endif
#ifndef _Const_
#define _Const_
#endif
#ifndef _Create_lock_level_
#define _Create_lock_level_(level)
#endif
#ifndef _Csalcat1_
#define _Csalcat1_(x,y)
#endif
#ifndef _Csalcat2_
#define _Csalcat2_(x,y)
#endif
#ifndef _Deref2_pre_readonly_
#define _Deref2_pre_readonly_
#endif
#ifndef _Deref_in_bound_
#define _Deref_in_bound_
#endif
#ifndef _Deref_in_range_
#define _Deref_in_range_(lb,ub)
#endif
#ifndef _Deref_inout_bound_
#define _Deref_inout_bound_
#endif
#ifndef _Deref_inout_z_
#define _Deref_inout_z_
#endif
#ifndef _Deref_inout_z_bytecap_c_
#define _Deref_inout_z_bytecap_c_(size)
#endif
#ifndef _Deref_inout_z_cap_c_
#define _Deref_inout_z_cap_c_(size)
#endif
#ifndef _Deref_opt_out_
#define _Deref_opt_out_
#endif
#ifndef _Deref_opt_out_opt_
#define _Deref_opt_out_opt_
#endif
#ifndef _Deref_opt_out_opt_z_
#define _Deref_opt_out_opt_z_
#endif
#ifndef _Deref_opt_out_z_
#define _Deref_opt_out_z_
#endif
#ifndef _Deref_out_
#define _Deref_out_
#endif
#ifndef _Deref_out_bound_
#define _Deref_out_bound_
#endif
#ifndef _Deref_out_opt_
#define _Deref_out_opt_
#endif
#ifndef _Deref_out_opt_z_
#define _Deref_out_opt_z_
#endif
#ifndef _Deref_out_range_
#define _Deref_out_range_(lb,ub)
#endif
#ifndef _Deref_out_z_
#define _Deref_out_z_
#endif
#ifndef _Deref_out_z_bytecap_c_
#define _Deref_out_z_bytecap_c_(size)
#endif
#ifndef _Deref_out_z_cap_c_
#define _Deref_out_z_cap_c_(size)
#endif
#ifndef _Deref_post_bytecap_
#define _Deref_post_bytecap_(size)
#endif
#ifndef _Deref_post_bytecap_c_
#define _Deref_post_bytecap_c_(size)
#endif
#ifndef _Deref_post_bytecap_x_
#define _Deref_post_bytecap_x_(size)
#endif
#ifndef _Deref_post_bytecount_
#define _Deref_post_bytecount_(size)
#endif
#ifndef _Deref_post_bytecount_c_
#define _Deref_post_bytecount_c_(size)
#endif
#ifndef _Deref_post_bytecount_x_
#define _Deref_post_bytecount_x_(size)
#endif
#ifndef _Deref_post_cap_
#define _Deref_post_cap_(size)
#endif
#ifndef _Deref_post_cap_c_
#define _Deref_post_cap_c_(size)
#endif
#ifndef _Deref_post_cap_x_
#define _Deref_post_cap_x_(size)
#endif
#ifndef _Deref_post_count_
#define _Deref_post_count_(size)
#endif
#ifndef _Deref_post_count_c_
#define _Deref_post_count_c_(size)
#endif
#ifndef _Deref_post_count_x_
#define _Deref_post_count_x_(size)
#endif
#ifndef _Deref_post_maybenull_
#define _Deref_post_maybenull_
#endif
#ifndef _Deref_post_notnull_
#define _Deref_post_notnull_
#endif
#ifndef _Deref_post_null_
#define _Deref_post_null_
#endif
#ifndef _Deref_post_opt_bytecap_
#define _Deref_post_opt_bytecap_(size)
#endif
#ifndef _Deref_post_opt_bytecap_c_
#define _Deref_post_opt_bytecap_c_(size)
#endif
#ifndef _Deref_post_opt_bytecap_x_
#define _Deref_post_opt_bytecap_x_(size)
#endif
#ifndef _Deref_post_opt_bytecount_
#define _Deref_post_opt_bytecount_(size)
#endif
#ifndef _Deref_post_opt_bytecount_c_
#define _Deref_post_opt_bytecount_c_(size)
#endif
#ifndef _Deref_post_opt_bytecount_x_
#define _Deref_post_opt_bytecount_x_(size)
#endif
#ifndef _Deref_post_opt_cap_
#define _Deref_post_opt_cap_(size)
#endif
#ifndef _Deref_post_opt_cap_c_
#define _Deref_post_opt_cap_c_(size)
#endif
#ifndef _Deref_post_opt_cap_x_
#define _Deref_post_opt_cap_x_(size)
#endif
#ifndef _Deref_post_opt_count_
#define _Deref_post_opt_count_(size)
#endif
#ifndef _Deref_post_opt_count_c_
#define _Deref_post_opt_count_c_(size)
#endif
#ifndef _Deref_post_opt_count_x_
#define _Deref_post_opt_count_x_(size)
#endif
#ifndef _Deref_post_opt_valid_
#define _Deref_post_opt_valid_
#endif
#ifndef _Deref_post_opt_valid_bytecap_
#define _Deref_post_opt_valid_bytecap_(size)
#endif
#ifndef _Deref_post_opt_valid_bytecap_c_
#define _Deref_post_opt_valid_bytecap_c_(size)
#endif
#ifndef _Deref_post_opt_valid_bytecap_x_
#define _Deref_post_opt_valid_bytecap_x_(size)
#endif
#ifndef _Deref_post_opt_valid_cap_
#define _Deref_post_opt_valid_cap_(size)
#endif
#ifndef _Deref_post_opt_valid_cap_c_
#define _Deref_post_opt_valid_cap_c_(size)
#endif
#ifndef _Deref_post_opt_valid_cap_x_
#define _Deref_post_opt_valid_cap_x_(size)
#endif
#ifndef _Deref_post_opt_z_
#define _Deref_post_opt_z_
#endif
#ifndef _Deref_post_opt_z_bytecap_
#define _Deref_post_opt_z_bytecap_(size)
#endif
#ifndef _Deref_post_opt_z_bytecap_c_
#define _Deref_post_opt_z_bytecap_c_(size)
#endif
#ifndef _Deref_post_opt_z_bytecap_x_
#define _Deref_post_opt_z_bytecap_x_(size)
#endif
#ifndef _Deref_post_opt_z_cap_
#define _Deref_post_opt_z_cap_(size)
#endif
#ifndef _Deref_post_opt_z_cap_c_
#define _Deref_post_opt_z_cap_c_(size)
#endif
#ifndef _Deref_post_opt_z_cap_x_
#define _Deref_post_opt_z_cap_x_(size)
#endif
#ifndef _Deref_post_valid_
#define _Deref_post_valid_
#endif
#ifndef _Deref_post_valid_bytecap_
#define _Deref_post_valid_bytecap_(size)
#endif
#ifndef _Deref_post_valid_bytecap_c_
#define _Deref_post_valid_bytecap_c_(size)
#endif
#ifndef _Deref_post_valid_bytecap_x_
#define _Deref_post_valid_bytecap_x_(size)
#endif
#ifndef _Deref_post_valid_cap_
#define _Deref_post_valid_cap_(size)
#endif
#ifndef _Deref_post_valid_cap_c_
#define _Deref_post_valid_cap_c_(size)
#endif
#ifndef _Deref_post_valid_cap_x_
#define _Deref_post_valid_cap_x_(size)
#endif
#ifndef _Deref_post_z_
#define _Deref_post_z_
#endif
#ifndef _Deref_post_z_bytecap_
#define _Deref_post_z_bytecap_(size)
#endif
#ifndef _Deref_post_z_bytecap_c_
#define _Deref_post_z_bytecap_c_(size)
#endif
#ifndef _Deref_post_z_bytecap_x_
#define _Deref_post_z_bytecap_x_(size)
#endif
#ifndef _Deref_post_z_cap_
#define _Deref_post_z_cap_(size)
#endif
#ifndef _Deref_post_z_cap_c_
#define _Deref_post_z_cap_c_(size)
#endif
#ifndef _Deref_post_z_cap_x_
#define _Deref_post_z_cap_x_(size)
#endif
#ifndef _Deref_pre_bytecap_
#define _Deref_pre_bytecap_(size)
#endif
#ifndef _Deref_pre_bytecap_c_
#define _Deref_pre_bytecap_c_(size)
#endif
#ifndef _Deref_pre_bytecap_x_
#define _Deref_pre_bytecap_x_(size)
#endif
#ifndef _Deref_pre_bytecount_
#define _Deref_pre_bytecount_(size)
#endif
#ifndef _Deref_pre_bytecount_c_
#define _Deref_pre_bytecount_c_(size)
#endif
#ifndef _Deref_pre_bytecount_x_
#define _Deref_pre_bytecount_x_(size)
#endif
#ifndef _Deref_pre_cap_
#define _Deref_pre_cap_(size)
#endif
#ifndef _Deref_pre_cap_c_
#define _Deref_pre_cap_c_(size)
#endif
#ifndef _Deref_pre_cap_x_
#define _Deref_pre_cap_x_(size)
#endif
#ifndef _Deref_pre_count_
#define _Deref_pre_count_(size)
#endif
#ifndef _Deref_pre_count_c_
#define _Deref_pre_count_c_(size)
#endif
#ifndef _Deref_pre_count_x_
#define _Deref_pre_count_x_(size)
#endif
#ifndef _Deref_pre_invalid_
#define _Deref_pre_invalid_
#endif
#ifndef _Deref_pre_maybenull_
#define _Deref_pre_maybenull_
#endif
#ifndef _Deref_pre_notnull_
#define _Deref_pre_notnull_
#endif
#ifndef _Deref_pre_null_
#define _Deref_pre_null_
#endif
#ifndef _Deref_pre_opt_bytecap_
#define _Deref_pre_opt_bytecap_(size)
#endif
#ifndef _Deref_pre_opt_bytecap_c_
#define _Deref_pre_opt_bytecap_c_(size)
#endif
#ifndef _Deref_pre_opt_bytecap_x_
#define _Deref_pre_opt_bytecap_x_(size)
#endif
#ifndef _Deref_pre_opt_bytecount_
#define _Deref_pre_opt_bytecount_(size)
#endif
#ifndef _Deref_pre_opt_bytecount_c_
#define _Deref_pre_opt_bytecount_c_(size)
#endif
#ifndef _Deref_pre_opt_bytecount_x_
#define _Deref_pre_opt_bytecount_x_(size)
#endif
#ifndef _Deref_pre_opt_cap_
#define _Deref_pre_opt_cap_(size)
#endif
#ifndef _Deref_pre_opt_cap_c_
#define _Deref_pre_opt_cap_c_(size)
#endif
#ifndef _Deref_pre_opt_cap_x_
#define _Deref_pre_opt_cap_x_(size)
#endif
#ifndef _Deref_pre_opt_count_
#define _Deref_pre_opt_count_(size)
#endif
#ifndef _Deref_pre_opt_count_c_
#define _Deref_pre_opt_count_c_(size)
#endif
#ifndef _Deref_pre_opt_count_x_
#define _Deref_pre_opt_count_x_(size)
#endif
#ifndef _Deref_pre_opt_valid_
#define _Deref_pre_opt_valid_
#endif
#ifndef _Deref_pre_opt_valid_bytecap_
#define _Deref_pre_opt_valid_bytecap_(size)
#endif
#ifndef _Deref_pre_opt_valid_bytecap_c_
#define _Deref_pre_opt_valid_bytecap_c_(size)
#endif
#ifndef _Deref_pre_opt_valid_bytecap_x_
#define _Deref_pre_opt_valid_bytecap_x_(size)
#endif
#ifndef _Deref_pre_opt_valid_cap_
#define _Deref_pre_opt_valid_cap_(size)
#endif
#ifndef _Deref_pre_opt_valid_cap_c_
#define _Deref_pre_opt_valid_cap_c_(size)
#endif
#ifndef _Deref_pre_opt_valid_cap_x_
#define _Deref_pre_opt_valid_cap_x_(size)
#endif
#ifndef _Deref_pre_opt_z_
#define _Deref_pre_opt_z_
#endif
#ifndef _Deref_pre_opt_z_bytecap_
#define _Deref_pre_opt_z_bytecap_(size)
#endif
#ifndef _Deref_pre_opt_z_bytecap_c_
#define _Deref_pre_opt_z_bytecap_c_(size)
#endif
#ifndef _Deref_pre_opt_z_bytecap_x_
#define _Deref_pre_opt_z_bytecap_x_(size)
#endif
#ifndef _Deref_pre_opt_z_cap_
#define _Deref_pre_opt_z_cap_(size)
#endif
#ifndef _Deref_pre_opt_z_cap_c_
#define _Deref_pre_opt_z_cap_c_(size)
#endif
#ifndef _Deref_pre_opt_z_cap_x_
#define _Deref_pre_opt_z_cap_x_(size)
#endif
#ifndef _Deref_pre_readonly_
#define _Deref_pre_readonly_
#endif
#ifndef _Deref_pre_valid_
#define _Deref_pre_valid_
#endif
#ifndef _Deref_pre_valid_bytecap_
#define _Deref_pre_valid_bytecap_(size)
#endif
#ifndef _Deref_pre_valid_bytecap_c_
#define _Deref_pre_valid_bytecap_c_(size)
#endif
#ifndef _Deref_pre_valid_bytecap_x_
#define _Deref_pre_valid_bytecap_x_(size)
#endif
#ifndef _Deref_pre_valid_cap_
#define _Deref_pre_valid_cap_(size)
#endif
#ifndef _Deref_pre_valid_cap_c_
#define _Deref_pre_valid_cap_c_(size)
#endif
#ifndef _Deref_pre_valid_cap_x_
#define _Deref_pre_valid_cap_x_(size)
#endif
#ifndef _Deref_pre_writeonly_
#define _Deref_pre_writeonly_
#endif
#ifndef _Deref_pre_z_
#define _Deref_pre_z_
#endif
#ifndef _Deref_pre_z_bytecap_
#define _Deref_pre_z_bytecap_(size)
#endif
#ifndef _Deref_pre_z_bytecap_c_
#define _Deref_pre_z_bytecap_c_(size)
#endif
#ifndef _Deref_pre_z_bytecap_x_
#define _Deref_pre_z_bytecap_x_(size)
#endif
#ifndef _Deref_pre_z_cap_
#define _Deref_pre_z_cap_(size)
#endif
#ifndef _Deref_pre_z_cap_c_
#define _Deref_pre_z_cap_c_(size)
#endif
#ifndef _Deref_pre_z_cap_x_
#define _Deref_pre_z_cap_x_(size)
#endif
#ifndef _Deref_prepost_bytecap_
#define _Deref_prepost_bytecap_(size)
#endif
#ifndef _Deref_prepost_bytecap_x_
#define _Deref_prepost_bytecap_x_(size)
#endif
#ifndef _Deref_prepost_bytecount_
#define _Deref_prepost_bytecount_(size)
#endif
#ifndef _Deref_prepost_bytecount_x_
#define _Deref_prepost_bytecount_x_(size)
#endif
#ifndef _Deref_prepost_cap_
#define _Deref_prepost_cap_(size)
#endif
#ifndef _Deref_prepost_cap_x_
#define _Deref_prepost_cap_x_(size)
#endif
#ifndef _Deref_prepost_count_
#define _Deref_prepost_count_(size)
#endif
#ifndef _Deref_prepost_count_x_
#define _Deref_prepost_count_x_(size)
#endif
#ifndef _Deref_prepost_opt_bytecap_
#define _Deref_prepost_opt_bytecap_(size)
#endif
#ifndef _Deref_prepost_opt_bytecap_x_
#define _Deref_prepost_opt_bytecap_x_(size)
#endif
#ifndef _Deref_prepost_opt_bytecount_
#define _Deref_prepost_opt_bytecount_(size)
#endif
#ifndef _Deref_prepost_opt_bytecount_x_
#define _Deref_prepost_opt_bytecount_x_(size)
#endif
#ifndef _Deref_prepost_opt_cap_
#define _Deref_prepost_opt_cap_(size)
#endif
#ifndef _Deref_prepost_opt_cap_x_
#define _Deref_prepost_opt_cap_x_(size)
#endif
#ifndef _Deref_prepost_opt_count_
#define _Deref_prepost_opt_count_(size)
#endif
#ifndef _Deref_prepost_opt_count_x_
#define _Deref_prepost_opt_count_x_(size)
#endif
#ifndef _Deref_prepost_opt_valid_
#define _Deref_prepost_opt_valid_
#endif
#ifndef _Deref_prepost_opt_valid_bytecap_
#define _Deref_prepost_opt_valid_bytecap_(size)
#endif
#ifndef _Deref_prepost_opt_valid_bytecap_x_
#define _Deref_prepost_opt_valid_bytecap_x_(size)
#endif
#ifndef _Deref_prepost_opt_valid_cap_
#define _Deref_prepost_opt_valid_cap_(size)
#endif
#ifndef _Deref_prepost_opt_valid_cap_x_
#define _Deref_prepost_opt_valid_cap_x_(size)
#endif
#ifndef _Deref_prepost_opt_z_
#define _Deref_prepost_opt_z_
#endif
#ifndef _Deref_prepost_opt_z_bytecap_
#define _Deref_prepost_opt_z_bytecap_(size)
#endif
#ifndef _Deref_prepost_opt_z_cap_
#define _Deref_prepost_opt_z_cap_(size)
#endif
#ifndef _Deref_prepost_valid_
#define _Deref_prepost_valid_
#endif
#ifndef _Deref_prepost_valid_bytecap_
#define _Deref_prepost_valid_bytecap_(size)
#endif
#ifndef _Deref_prepost_valid_bytecap_x_
#define _Deref_prepost_valid_bytecap_x_(size)
#endif
#ifndef _Deref_prepost_valid_cap_
#define _Deref_prepost_valid_cap_(size)
#endif
#ifndef _Deref_prepost_valid_cap_x_
#define _Deref_prepost_valid_cap_x_(size)
#endif
#ifndef _Deref_prepost_z_
#define _Deref_prepost_z_
#endif
#ifndef _Deref_prepost_z_bytecap_
#define _Deref_prepost_z_bytecap_(size)
#endif
#ifndef _Deref_prepost_z_cap_
#define _Deref_prepost_z_cap_(size)
#endif
#ifndef _Deref_ret_bound_
#define _Deref_ret_bound_
#endif
#ifndef _Deref_ret_opt_z_
#define _Deref_ret_opt_z_
#endif
#ifndef _Deref_ret_range_
#define _Deref_ret_range_(lb,ub)
#endif
#ifndef _Deref_ret_z_
#define _Deref_ret_z_
#endif
#ifndef _Enum_is_bitflag_
#define _Enum_is_bitflag_
#endif
#ifndef _Field_range_
#define _Field_range_(min,max)
#endif
#ifndef _Field_size_
#define _Field_size_(size)
#endif
#ifndef _Field_size_bytes_
#define _Field_size_bytes_(size)
#endif
#ifndef _Field_size_bytes_full_
#define _Field_size_bytes_full_(size)
#endif
#ifndef _Field_size_bytes_full_opt_
#define _Field_size_bytes_full_opt_(size)
#endif
#ifndef _Field_size_bytes_opt_
#define _Field_size_bytes_opt_(size)
#endif
#ifndef _Field_size_bytes_part_
#define _Field_size_bytes_part_(size, count)
#endif
#ifndef _Field_size_bytes_part_opt_
#define _Field_size_bytes_part_opt_(size, count)
#endif
#ifndef _Field_size_full_
#define _Field_size_full_(size)
#endif
#ifndef _Field_size_full_opt_
#define _Field_size_full_opt_(size)
#endif
#ifndef _Field_size_opt_
#define _Field_size_opt_(size)
#endif
#ifndef _Field_size_part_
#define _Field_size_part_(size, count)
#endif
#ifndef _Field_size_part_opt_
#define _Field_size_part_opt_(size, count)
#endif
#ifndef _Field_z_
#define _Field_z_
#endif
#ifndef _Function_class_
#define _Function_class_(x)
#endif
#ifndef _Function_ignore_lock_checking_
#define _Function_ignore_lock_checking_(lock)
#endif
#ifndef _GrouP_
#define _GrouP_(annos)
#endif
#ifndef _Group_
#define _Group_(annos)
#endif
#ifndef _Guarded_by_
#define _Guarded_by_(lock)
#endif
#ifndef _Has_lock_kind_
#define _Has_lock_kind_(kind)
#endif
#ifndef _Has_lock_level_
#define _Has_lock_level_(level)
#endif
#ifndef _In_
#define _In_
#endif
#ifndef _In_bound_
#define _In_bound_
#endif
#ifndef _In_bytecount_
#define _In_bytecount_(size)
#endif
#ifndef _In_bytecount_c_
#define _In_bytecount_c_(size)
#endif
#ifndef _In_bytecount_x_
#define _In_bytecount_x_(size)
#endif
#ifndef _In_count_
#define _In_count_(size)
#endif
#ifndef _In_count_c_
#define _In_count_c_(size)
#endif
#ifndef _In_count_x_
#define _In_count_x_(size)
#endif
#ifndef _In_defensive_
#define _In_defensive_(annotes)
#endif
#ifndef _In_function_class_
#define _In_function_class_(x)
#endif
#ifndef _In_opt_
#define _In_opt_
#endif
#ifndef _In_opt_bytecount_
#define _In_opt_bytecount_(size)
#endif
#ifndef _In_opt_bytecount_c_
#define _In_opt_bytecount_c_(size)
#endif
#ifndef _In_opt_bytecount_x_
#define _In_opt_bytecount_x_(size)
#endif
#ifndef _In_opt_count_
#define _In_opt_count_(size)
#endif
#ifndef _In_opt_count_c_
#define _In_opt_count_c_(size)
#endif
#ifndef _In_opt_count_x_
#define _In_opt_count_x_(size)
#endif
#ifndef _In_opt_ptrdiff_count_
#define _In_opt_ptrdiff_count_(size)
#endif
#ifndef _In_opt_z_
#define _In_opt_z_
#endif
#ifndef _In_opt_z_bytecount_
#define _In_opt_z_bytecount_(size)
#endif
#ifndef _In_opt_z_bytecount_c_
#define _In_opt_z_bytecount_c_(size)
#endif
#ifndef _In_opt_z_count_
#define _In_opt_z_count_(size)
#endif
#ifndef _In_opt_z_count_c_
#define _In_opt_z_count_c_(size)
#endif
#ifndef _In_ptrdiff_count_
#define _In_ptrdiff_count_(size)
#endif
#ifndef _In_range_
#define _In_range_(lb,ub)
#endif
#ifndef _In_reads_
#define _In_reads_(size)
#endif
#ifndef _In_reads_bytes_
#define _In_reads_bytes_(size)
#endif
#ifndef _In_reads_bytes_opt_
#define _In_reads_bytes_opt_(size)
#endif
#ifndef _In_reads_opt_
#define _In_reads_opt_(size)
#endif
#ifndef _In_reads_opt_z_
#define _In_reads_opt_z_(size)
#endif
#ifndef _In_reads_or_z_
#define _In_reads_or_z_(size)
#endif
#ifndef _In_reads_or_z_opt_
#define _In_reads_or_z_opt_(size)
#endif
#ifndef _In_reads_to_ptr_
#define _In_reads_to_ptr_(ptr)
#endif
#ifndef _In_reads_to_ptr_opt_
#define _In_reads_to_ptr_opt_(ptr)
#endif
#ifndef _In_reads_to_ptr_opt_z_
#define _In_reads_to_ptr_opt_z_(ptr)
#endif
#ifndef _In_reads_to_ptr_z_
#define _In_reads_to_ptr_z_(ptr)
#endif
#ifndef _In_reads_z_
#define _In_reads_z_(size)
#endif
#ifndef _In_z_
#define _In_z_
#endif
#ifndef _In_z_bytecount_
#define _In_z_bytecount_(size)
#endif
#ifndef _In_z_bytecount_c_
#define _In_z_bytecount_c_(size)
#endif
#ifndef _In_z_count_
#define _In_z_count_(size)
#endif
#ifndef _In_z_count_c_
#define _In_z_count_c_(size)
#endif
#ifndef _Inout_
#define _Inout_
#endif
#ifndef _Inout_bytecap_
#define _Inout_bytecap_(size)
#endif
#ifndef _Inout_bytecap_c_
#define _Inout_bytecap_c_(size)
#endif
#ifndef _Inout_bytecap_x_
#define _Inout_bytecap_x_(size)
#endif
#ifndef _Inout_bytecount_
#define _Inout_bytecount_(size)
#endif
#ifndef _Inout_bytecount_c_
#define _Inout_bytecount_c_(size)
#endif
#ifndef _Inout_bytecount_x_
#define _Inout_bytecount_x_(size)
#endif
#ifndef _Inout_cap_
#define _Inout_cap_(size)
#endif
#ifndef _Inout_cap_c_
#define _Inout_cap_c_(size)
#endif
#ifndef _Inout_cap_x_
#define _Inout_cap_x_(size)
#endif
#ifndef _Inout_count_
#define _Inout_count_(size)
#endif
#ifndef _Inout_count_c_
#define _Inout_count_c_(size)
#endif
#ifndef _Inout_count_x_
#define _Inout_count_x_(size)
#endif
#ifndef _Inout_defensive_
#define _Inout_defensive_(annotes)
#endif
#ifndef _Inout_opt_
#define _Inout_opt_
#endif
#ifndef _Inout_opt_bytecap_
#define _Inout_opt_bytecap_(size)
#endif
#ifndef _Inout_opt_bytecap_c_
#define _Inout_opt_bytecap_c_(size)
#endif
#ifndef _Inout_opt_bytecap_x_
#define _Inout_opt_bytecap_x_(size)
#endif
#ifndef _Inout_opt_bytecount_
#define _Inout_opt_bytecount_(size)
#endif
#ifndef _Inout_opt_bytecount_c_
#define _Inout_opt_bytecount_c_(size)
#endif
#ifndef _Inout_opt_bytecount_x_
#define _Inout_opt_bytecount_x_(size)
#endif
#ifndef _Inout_opt_cap_
#define _Inout_opt_cap_(size)
#endif
#ifndef _Inout_opt_cap_c_
#define _Inout_opt_cap_c_(size)
#endif
#ifndef _Inout_opt_cap_x_
#define _Inout_opt_cap_x_(size)
#endif
#ifndef _Inout_opt_count_
#define _Inout_opt_count_(size)
#endif
#ifndef _Inout_opt_count_c_
#define _Inout_opt_count_c_(size)
#endif
#ifndef _Inout_opt_count_x_
#define _Inout_opt_count_x_(size)
#endif
#ifndef _Inout_opt_ptrdiff_count_
#define _Inout_opt_ptrdiff_count_(size)
#endif
#ifndef _Inout_opt_z_
#define _Inout_opt_z_
#endif
#ifndef _Inout_opt_z_bytecap_
#define _Inout_opt_z_bytecap_(size)
#endif
#ifndef _Inout_opt_z_bytecap_c_
#define _Inout_opt_z_bytecap_c_(size)
#endif
#ifndef _Inout_opt_z_bytecap_x_
#define _Inout_opt_z_bytecap_x_(size)
#endif
#ifndef _Inout_opt_z_bytecount_
#define _Inout_opt_z_bytecount_(size)
#endif
#ifndef _Inout_opt_z_bytecount_c_
#define _Inout_opt_z_bytecount_c_(size)
#endif
#ifndef _Inout_opt_z_cap_
#define _Inout_opt_z_cap_(size)
#endif
#ifndef _Inout_opt_z_cap_c_
#define _Inout_opt_z_cap_c_(size)
#endif
#ifndef _Inout_opt_z_cap_x_
#define _Inout_opt_z_cap_x_(size)
#endif
#ifndef _Inout_opt_z_count_
#define _Inout_opt_z_count_(size)
#endif
#ifndef _Inout_opt_z_count_c_
#define _Inout_opt_z_count_c_(size)
#endif
#ifndef _Inout_ptrdiff_count_
#define _Inout_ptrdiff_count_(size)
#endif
#ifndef _Inout_updates_
#define _Inout_updates_(size)
#endif
#ifndef _Inout_updates_all_
#define _Inout_updates_all_(size)
#endif
#ifndef _Inout_updates_all_opt_
#define _Inout_updates_all_opt_(size)
#endif
#ifndef _Inout_updates_bytes_
#define _Inout_updates_bytes_(size)
#endif
#ifndef _Inout_updates_bytes_all_
#define _Inout_updates_bytes_all_(size)
#endif
#ifndef _Inout_updates_bytes_all_opt_
#define _Inout_updates_bytes_all_opt_(size)
#endif
#ifndef _Inout_updates_bytes_opt_
#define _Inout_updates_bytes_opt_(size)
#endif
#ifndef _Inout_updates_bytes_to_
#define _Inout_updates_bytes_to_(size,count)
#endif
#ifndef _Inout_updates_bytes_to_opt_
#define _Inout_updates_bytes_to_opt_(size,count)
#endif
#ifndef _Inout_updates_opt_
#define _Inout_updates_opt_(size)
#endif
#ifndef _Inout_updates_opt_z_
#define _Inout_updates_opt_z_(size)
#endif
#ifndef _Inout_updates_to_
#define _Inout_updates_to_(size,count)
#endif
#ifndef _Inout_updates_to_opt_
#define _Inout_updates_to_opt_(size,count)
#endif
#ifndef _Inout_updates_z_
#define _Inout_updates_z_(size)
#endif
#ifndef _Inout_z_
#define _Inout_z_
#endif
#ifndef _Inout_z_bytecap_
#define _Inout_z_bytecap_(size)
#endif
#ifndef _Inout_z_bytecap_c_
#define _Inout_z_bytecap_c_(size)
#endif
#ifndef _Inout_z_bytecap_x_
#define _Inout_z_bytecap_x_(size)
#endif
#ifndef _Inout_z_bytecount_
#define _Inout_z_bytecount_(size)
#endif
#ifndef _Inout_z_bytecount_c_
#define _Inout_z_bytecount_c_(size)
#endif
#ifndef _Inout_z_cap_
#define _Inout_z_cap_(size)
#endif
#ifndef _Inout_z_cap_c_
#define _Inout_z_cap_c_(size)
#endif
#ifndef _Inout_z_cap_x_
#define _Inout_z_cap_x_(size)
#endif
#ifndef _Inout_z_count_
#define _Inout_z_count_(size)
#endif
#ifndef _Inout_z_count_c_
#define _Inout_z_count_c_(size)
#endif
#ifndef _Interlocked_
#define _Interlocked_
#endif
#ifndef _Interlocked_operand_
#define _Interlocked_operand_
#endif
#ifndef _Internal_lock_level_order_
#define _Internal_lock_level_order_(a,b)
#endif
#ifndef _Internal_set_lock_count_
#define _Internal_set_lock_count_(lock, count)
#endif
#ifndef _Literal_
#define _Literal_
#endif
#ifndef _Lock_level_order_
#define _Lock_level_order_(a,b)
#endif
#ifndef _Maybe_raises_SEH_exception_
#define _Maybe_raises_SEH_exception_
#endif
#ifndef _Maybenull_
#define _Maybenull_
#endif
#ifndef _Maybevalid_
#define _Maybevalid_
#endif
#ifndef _Must_inspect_result_
#define _Must_inspect_result_ __attribute_warn_unused_result__
#endif
#ifndef _No_competing_thread_
#define _No_competing_thread_
#endif
#ifndef _No_competing_thread_begin_
#define _No_competing_thread_begin_
#endif
#ifndef _No_competing_thread_end_
#define _No_competing_thread_end_
#endif
#ifndef _Notliteral_
#define _Notliteral_
#endif
#ifndef _Notnull_
#define _Notnull_
#endif
#ifndef _Notref_
#define _Notref_
#endif
#ifndef _Notvalid_
#define _Notvalid_
#endif
#ifndef _NullNull_terminated_
#define _NullNull_terminated_
#endif
#ifndef _Null_
#define _Null_
#endif
#ifndef _Null_terminated_
#define _Null_terminated_
#endif
#ifndef _On_failure_
#define _On_failure_(annos)
#endif
#ifndef _Out_
#define _Out_
#endif
#ifndef _Out_bound_
#define _Out_bound_
#endif
#ifndef _Out_bytecap_
#define _Out_bytecap_(size)
#endif
#ifndef _Out_bytecap_c_
#define _Out_bytecap_c_(size)
#endif
#ifndef _Out_bytecap_post_bytecount_
#define _Out_bytecap_post_bytecount_(cap,count)
#endif
#ifndef _Out_bytecap_x_
#define _Out_bytecap_x_(size)
#endif
#ifndef _Out_bytecapcount_
#define _Out_bytecapcount_(capcount)
#endif
#ifndef _Out_bytecapcount_x_
#define _Out_bytecapcount_x_(capcount)
#endif
#ifndef _Out_cap_
#define _Out_cap_(size)
#endif
#ifndef _Out_cap_c_
#define _Out_cap_c_(size)
#endif
#ifndef _Out_cap_m_
#define _Out_cap_m_(mult,size)
#endif
#ifndef _Out_cap_post_count_
#define _Out_cap_post_count_(cap,count)
#endif
#ifndef _Out_cap_x_
#define _Out_cap_x_(size)
#endif
#ifndef _Out_capcount_
#define _Out_capcount_(capcount)
#endif
#ifndef _Out_capcount_x_
#define _Out_capcount_x_(capcount)
#endif
#ifndef _Out_defensive_
#define _Out_defensive_(annotes)
#endif
#ifndef _Out_opt_
#define _Out_opt_
#endif
#ifndef _Out_opt_bytecap_
#define _Out_opt_bytecap_(size)
#endif
#ifndef _Out_opt_bytecap_c_
#define _Out_opt_bytecap_c_(size)
#endif
#ifndef _Out_opt_bytecap_post_bytecount_
#define _Out_opt_bytecap_post_bytecount_(cap,count)
#endif
#ifndef _Out_opt_bytecap_x_
#define _Out_opt_bytecap_x_(size)
#endif
#ifndef _Out_opt_bytecapcount_
#define _Out_opt_bytecapcount_(capcount)
#endif
#ifndef _Out_opt_bytecapcount_x_
#define _Out_opt_bytecapcount_x_(capcount)
#endif
#ifndef _Out_opt_cap_
#define _Out_opt_cap_(size)
#endif
#ifndef _Out_opt_cap_c_
#define _Out_opt_cap_c_(size)
#endif
#ifndef _Out_opt_cap_m_
#define _Out_opt_cap_m_(mult,size)
#endif
#ifndef _Out_opt_cap_post_count_
#define _Out_opt_cap_post_count_(cap,count)
#endif
#ifndef _Out_opt_cap_x_
#define _Out_opt_cap_x_(size)
#endif
#ifndef _Out_opt_capcount_
#define _Out_opt_capcount_(capcount)
#endif
#ifndef _Out_opt_capcount_x_
#define _Out_opt_capcount_x_(capcount)
#endif
#ifndef _Out_opt_ptrdiff_cap_
#define _Out_opt_ptrdiff_cap_(size)
#endif
#ifndef _Out_opt_z_bytecap_
#define _Out_opt_z_bytecap_(size)
#endif
#ifndef _Out_opt_z_bytecap_c_
#define _Out_opt_z_bytecap_c_(size)
#endif
#ifndef _Out_opt_z_bytecap_post_bytecount_
#define _Out_opt_z_bytecap_post_bytecount_(cap,count)
#endif
#ifndef _Out_opt_z_bytecap_x_
#define _Out_opt_z_bytecap_x_(size)
#endif
#ifndef _Out_opt_z_bytecapcount_
#define _Out_opt_z_bytecapcount_(capcount)
#endif
#ifndef _Out_opt_z_cap_
#define _Out_opt_z_cap_(size)
#endif
#ifndef _Out_opt_z_cap_c_
#define _Out_opt_z_cap_c_(size)
#endif
#ifndef _Out_opt_z_cap_m_
#define _Out_opt_z_cap_m_(mult,size)
#endif
#ifndef _Out_opt_z_cap_post_count_
#define _Out_opt_z_cap_post_count_(cap,count)
#endif
#ifndef _Out_opt_z_cap_x_
#define _Out_opt_z_cap_x_(size)
#endif
#ifndef _Out_opt_z_capcount_
#define _Out_opt_z_capcount_(capcount)
#endif
#ifndef _Out_ptrdiff_cap_
#define _Out_ptrdiff_cap_(size)
#endif
#ifndef _Out_range_
#define _Out_range_(lb,ub)
#endif
#ifndef _Out_writes_
#define _Out_writes_(size)
#endif
#ifndef _Out_writes_all_
#define _Out_writes_all_(size)
#endif
#ifndef _Out_writes_all_opt_
#define _Out_writes_all_opt_(size)
#endif
#ifndef _Out_writes_bytes_
#define _Out_writes_bytes_(size)
#endif
#ifndef _Out_writes_bytes_all_
#define _Out_writes_bytes_all_(size)
#endif
#ifndef _Out_writes_bytes_all_opt_
#define _Out_writes_bytes_all_opt_(size)
#endif
#ifndef _Out_writes_bytes_opt_
#define _Out_writes_bytes_opt_(size)
#endif
#ifndef _Out_writes_bytes_to_
#define _Out_writes_bytes_to_(size,count)
#endif
#ifndef _Out_writes_bytes_to_opt_
#define _Out_writes_bytes_to_opt_(size,count)
#endif
#ifndef _Out_writes_opt_
#define _Out_writes_opt_(size)
#endif
#ifndef _Out_writes_opt_z_
#define _Out_writes_opt_z_(size)
#endif
#ifndef _Out_writes_to_
#define _Out_writes_to_(size,count)
#endif
#ifndef _Out_writes_to_opt_
#define _Out_writes_to_opt_(size,count)
#endif
#ifndef _Out_writes_to_ptr_
#define _Out_writes_to_ptr_(ptr)
#endif
#ifndef _Out_writes_to_ptr_opt_
#define _Out_writes_to_ptr_opt_(ptr)
#endif
#ifndef _Out_writes_to_ptr_opt_z_
#define _Out_writes_to_ptr_opt_z_(ptr)
#endif
#ifndef _Out_writes_to_ptr_z_
#define _Out_writes_to_ptr_z_(ptr)
#endif
#ifndef _Out_writes_z_
#define _Out_writes_z_(size)
#endif
#ifndef _Out_z_bytecap_
#define _Out_z_bytecap_(size)
#endif
#ifndef _Out_z_bytecap_c_
#define _Out_z_bytecap_c_(size)
#endif
#ifndef _Out_z_bytecap_post_bytecount_
#define _Out_z_bytecap_post_bytecount_(cap,count)
#endif
#ifndef _Out_z_bytecap_x_
#define _Out_z_bytecap_x_(size)
#endif
#ifndef _Out_z_bytecapcount_
#define _Out_z_bytecapcount_(capcount)
#endif
#ifndef _Out_z_cap_
#define _Out_z_cap_(size)
#endif
#ifndef _Out_z_cap_c_
#define _Out_z_cap_c_(size)
#endif
#ifndef _Out_z_cap_m_
#define _Out_z_cap_m_(mult,size)
#endif
#ifndef _Out_z_cap_post_count_
#define _Out_z_cap_post_count_(cap,count)
#endif
#ifndef _Out_z_cap_x_
#define _Out_z_cap_x_(size)
#endif
#ifndef _Out_z_capcount_
#define _Out_z_capcount_(capcount)
#endif
#ifndef _Outptr_
#define _Outptr_
#endif
#ifndef _Outptr_opt_
#define _Outptr_opt_
#endif
#ifndef _Outptr_opt_result_buffer_
#define _Outptr_opt_result_buffer_(size)
#endif
#ifndef _Outptr_opt_result_buffer_all_
#define _Outptr_opt_result_buffer_all_(size)
#endif
#ifndef _Outptr_opt_result_buffer_all_maybenull_
#define _Outptr_opt_result_buffer_all_maybenull_(size)
#endif
#ifndef _Outptr_opt_result_buffer_maybenull_
#define _Outptr_opt_result_buffer_maybenull_(size)
#endif
#ifndef _Outptr_opt_result_buffer_to_
#define _Outptr_opt_result_buffer_to_(size, count)
#endif
#ifndef _Outptr_opt_result_buffer_to_maybenull_
#define _Outptr_opt_result_buffer_to_maybenull_(size, count)
#endif
#ifndef _Outptr_opt_result_bytebuffer_
#define _Outptr_opt_result_bytebuffer_(size)
#endif
#ifndef _Outptr_opt_result_bytebuffer_all_
#define _Outptr_opt_result_bytebuffer_all_(size)
#endif
#ifndef _Outptr_opt_result_bytebuffer_all_maybenull_
#define _Outptr_opt_result_bytebuffer_all_maybenull_(size)
#endif
#ifndef _Outptr_opt_result_bytebuffer_maybenull_
#define _Outptr_opt_result_bytebuffer_maybenull_(size)
#endif
#ifndef _Outptr_opt_result_bytebuffer_to_
#define _Outptr_opt_result_bytebuffer_to_(size, count)
#endif
#ifndef _Outptr_opt_result_bytebuffer_to_maybenull_
#define _Outptr_opt_result_bytebuffer_to_maybenull_(size, count)
#endif
#ifndef _Outptr_opt_result_maybenull_
#define _Outptr_opt_result_maybenull_
#endif
#ifndef _Outptr_opt_result_maybenull_z_
#define _Outptr_opt_result_maybenull_z_
#endif
#ifndef _Outptr_opt_result_nullonfailure_
#define _Outptr_opt_result_nullonfailure_
#endif
#ifndef _Outptr_opt_result_z_
#define _Outptr_opt_result_z_
#endif
#ifndef _Outptr_result_buffer_
#define _Outptr_result_buffer_(size)
#endif
#ifndef _Outptr_result_buffer_all_
#define _Outptr_result_buffer_all_(size)
#endif
#ifndef _Outptr_result_buffer_all_maybenull_
#define _Outptr_result_buffer_all_maybenull_(size)
#endif
#ifndef _Outptr_result_buffer_maybenull_
#define _Outptr_result_buffer_maybenull_(size)
#endif
#ifndef _Outptr_result_buffer_to_
#define _Outptr_result_buffer_to_(size, count)
#endif
#ifndef _Outptr_result_buffer_to_maybenull_
#define _Outptr_result_buffer_to_maybenull_(size, count)
#endif
#ifndef _Outptr_result_bytebuffer_
#define _Outptr_result_bytebuffer_(size)
#endif
#ifndef _Outptr_result_bytebuffer_all_
#define _Outptr_result_bytebuffer_all_(size)
#endif
#ifndef _Outptr_result_bytebuffer_all_maybenull_
#define _Outptr_result_bytebuffer_all_maybenull_(size)
#endif
#ifndef _Outptr_result_bytebuffer_maybenull_
#define _Outptr_result_bytebuffer_maybenull_(size)
#endif
#ifndef _Outptr_result_bytebuffer_to_
#define _Outptr_result_bytebuffer_to_(size, count)
#endif
#ifndef _Outptr_result_bytebuffer_to_maybenull_
#define _Outptr_result_bytebuffer_to_maybenull_(size, count)
#endif
#ifndef _Outptr_result_maybenull_
#define _Outptr_result_maybenull_
#endif
#ifndef _Outptr_result_maybenull_z_
#define _Outptr_result_maybenull_z_
#endif
#ifndef _Outptr_result_nullonfailure_
#define _Outptr_result_nullonfailure_
#endif
#ifndef _Outptr_result_z_
#define _Outptr_result_z_
#endif
#ifndef _Outref_
#define _Outref_
#endif
#ifndef _Outref_result_buffer_
#define _Outref_result_buffer_(size)
#endif
#ifndef _Outref_result_buffer_all_
#define _Outref_result_buffer_all_(size)
#endif
#ifndef _Outref_result_buffer_all_maybenull_
#define _Outref_result_buffer_all_maybenull_(size)
#endif
#ifndef _Outref_result_buffer_maybenull_
#define _Outref_result_buffer_maybenull_(size)
#endif
#ifndef _Outref_result_buffer_to_
#define _Outref_result_buffer_to_(size, count)
#endif
#ifndef _Outref_result_buffer_to_maybenull_
#define _Outref_result_buffer_to_maybenull_(size, count)
#endif
#ifndef _Outref_result_bytebuffer_
#define _Outref_result_bytebuffer_(size)
#endif
#ifndef _Outref_result_bytebuffer_all_
#define _Outref_result_bytebuffer_all_(size)
#endif
#ifndef _Outref_result_bytebuffer_all_maybenull_
#define _Outref_result_bytebuffer_all_maybenull_(size)
#endif
#ifndef _Outref_result_bytebuffer_maybenull_
#define _Outref_result_bytebuffer_maybenull_(size)
#endif
#ifndef _Outref_result_bytebuffer_to_
#define _Outref_result_bytebuffer_to_(size, count)
#endif
#ifndef _Outref_result_bytebuffer_to_maybenull_
#define _Outref_result_bytebuffer_to_maybenull_(size, count)
#endif
#ifndef _Outref_result_maybenull_
#define _Outref_result_maybenull_
#endif
#ifndef _Outref_result_nullonfailure_
#define _Outref_result_nullonfailure_
#endif
#ifndef _Points_to_data_
#define _Points_to_data_
#endif
#ifndef _Post_
#define _Post_
#endif
#ifndef _Post_bytecap_
#define _Post_bytecap_(size)
#endif
#ifndef _Post_bytecount_
#define _Post_bytecount_(size)
#endif
#ifndef _Post_bytecount_c_
#define _Post_bytecount_c_(size)
#endif
#ifndef _Post_bytecount_x_
#define _Post_bytecount_x_(size)
#endif
#ifndef _Post_cap_
#define _Post_cap_(size)
#endif
#ifndef _Post_count_
#define _Post_count_(size)
#endif
#ifndef _Post_count_c_
#define _Post_count_c_(size)
#endif
#ifndef _Post_count_x_
#define _Post_count_x_(size)
#endif
#ifndef _Post_defensive_
#define _Post_defensive_
#endif
#ifndef _Post_equal_to_
#define _Post_equal_to_(expr)
#endif
#ifndef _Post_invalid_
#define _Post_invalid_
#endif
#ifndef _Post_maybenull_
#define _Post_maybenull_
#endif
#ifndef _Post_maybez_
#define _Post_maybez_
#endif
#ifndef _Post_notnull_
#define _Post_notnull_
#endif
#ifndef _Post_null_
#define _Post_null_
#endif
#ifndef _Post_ptr_invalid_
#define _Post_ptr_invalid_
#endif
#ifndef _Post_readable_byte_size_
#define _Post_readable_byte_size_(size)
#endif
#ifndef _Post_readable_size_
#define _Post_readable_size_(size)
#endif
#ifndef _Post_same_lock_
#define _Post_same_lock_(lock1,lock2)
#endif
#ifndef _Post_satisfies_
#define _Post_satisfies_(cond)
#endif
#ifndef _Post_valid_
#define _Post_valid_
#endif
#ifndef _Post_writable_byte_size_
#define _Post_writable_byte_size_(size)
#endif
#ifndef _Post_writable_size_
#define _Post_writable_size_(size)
#endif
#ifndef _Post_z_
#define _Post_z_
#endif
#ifndef _Post_z_bytecount_
#define _Post_z_bytecount_(size)
#endif
#ifndef _Post_z_bytecount_c_
#define _Post_z_bytecount_c_(size)
#endif
#ifndef _Post_z_bytecount_x_
#define _Post_z_bytecount_x_(size)
#endif
#ifndef _Post_z_count_
#define _Post_z_count_(size)
#endif
#ifndef _Post_z_count_c_
#define _Post_z_count_c_(size)
#endif
#ifndef _Post_z_count_x_
#define _Post_z_count_x_(size)
#endif
#ifndef _Pre_
#define _Pre_
#endif
#ifndef _Pre_bytecap_
#define _Pre_bytecap_(size)
#endif
#ifndef _Pre_bytecap_c_
#define _Pre_bytecap_c_(size)
#endif
#ifndef _Pre_bytecap_x_
#define _Pre_bytecap_x_(size)
#endif
#ifndef _Pre_bytecount_
#define _Pre_bytecount_(size)
#endif
#ifndef _Pre_bytecount_c_
#define _Pre_bytecount_c_(size)
#endif
#ifndef _Pre_bytecount_x_
#define _Pre_bytecount_x_(size)
#endif
#ifndef _Pre_cap_
#define _Pre_cap_(size)
#endif
#ifndef _Pre_cap_c_
#define _Pre_cap_c_(size)
#endif
#ifndef _Pre_cap_c_one_
#define _Pre_cap_c_one_
#endif
#ifndef _Pre_cap_for_
#define _Pre_cap_for_(param)
#endif
#ifndef _Pre_cap_m_
#define _Pre_cap_m_(mult,size)
#endif
#ifndef _Pre_cap_x_
#define _Pre_cap_x_(size)
#endif
#ifndef _Pre_count_
#define _Pre_count_(size)
#endif
#ifndef _Pre_count_c_
#define _Pre_count_c_(size)
#endif
#ifndef _Pre_count_x_
#define _Pre_count_x_(size)
#endif
#ifndef _Pre_defensive_
#define _Pre_defensive_
#endif
#ifndef _Pre_equal_to_
#define _Pre_equal_to_(expr)
#endif
#ifndef _Pre_invalid_
#define _Pre_invalid_
#endif
#ifndef _Pre_maybenull_
#define _Pre_maybenull_
#endif
#ifndef _Pre_notnull_
#define _Pre_notnull_
#endif
#ifndef _Pre_null_
#define _Pre_null_
#endif
#ifndef _Pre_opt_bytecap_
#define _Pre_opt_bytecap_(size)
#endif
#ifndef _Pre_opt_bytecap_c_
#define _Pre_opt_bytecap_c_(size)
#endif
#ifndef _Pre_opt_bytecap_x_
#define _Pre_opt_bytecap_x_(size)
#endif
#ifndef _Pre_opt_bytecount_
#define _Pre_opt_bytecount_(size)
#endif
#ifndef _Pre_opt_bytecount_c_
#define _Pre_opt_bytecount_c_(size)
#endif
#ifndef _Pre_opt_bytecount_x_
#define _Pre_opt_bytecount_x_(size)
#endif
#ifndef _Pre_opt_cap_
#define _Pre_opt_cap_(size)
#endif
#ifndef _Pre_opt_cap_c_
#define _Pre_opt_cap_c_(size)
#endif
#ifndef _Pre_opt_cap_c_one_
#define _Pre_opt_cap_c_one_
#endif
#ifndef _Pre_opt_cap_for_
#define _Pre_opt_cap_for_(param)
#endif
#ifndef _Pre_opt_cap_m_
#define _Pre_opt_cap_m_(mult,size)
#endif
#ifndef _Pre_opt_cap_x_
#define _Pre_opt_cap_x_(size)
#endif
#ifndef _Pre_opt_count_
#define _Pre_opt_count_(size)
#endif
#ifndef _Pre_opt_count_c_
#define _Pre_opt_count_c_(size)
#endif
#ifndef _Pre_opt_count_x_
#define _Pre_opt_count_x_(size)
#endif
#ifndef _Pre_opt_ptrdiff_cap_
#define _Pre_opt_ptrdiff_cap_(ptr)
#endif
#ifndef _Pre_opt_ptrdiff_count_
#define _Pre_opt_ptrdiff_count_(ptr)
#endif
#ifndef _Pre_opt_valid_
#define _Pre_opt_valid_
#endif
#ifndef _Pre_opt_valid_bytecap_
#define _Pre_opt_valid_bytecap_(size)
#endif
#ifndef _Pre_opt_valid_bytecap_c_
#define _Pre_opt_valid_bytecap_c_(size)
#endif
#ifndef _Pre_opt_valid_bytecap_x_
#define _Pre_opt_valid_bytecap_x_(size)
#endif
#ifndef _Pre_opt_valid_cap_
#define _Pre_opt_valid_cap_(size)
#endif
#ifndef _Pre_opt_valid_cap_c_
#define _Pre_opt_valid_cap_c_(size)
#endif
#ifndef _Pre_opt_valid_cap_x_
#define _Pre_opt_valid_cap_x_(size)
#endif
#ifndef _Pre_opt_z_
#define _Pre_opt_z_
#endif
#ifndef _Pre_opt_z_bytecap_
#define _Pre_opt_z_bytecap_(size)
#endif
#ifndef _Pre_opt_z_bytecap_c_
#define _Pre_opt_z_bytecap_c_(size)
#endif
#ifndef _Pre_opt_z_bytecap_x_
#define _Pre_opt_z_bytecap_x_(size)
#endif
#ifndef _Pre_opt_z_cap_
#define _Pre_opt_z_cap_(size)
#endif
#ifndef _Pre_opt_z_cap_c_
#define _Pre_opt_z_cap_c_(size)
#endif
#ifndef _Pre_opt_z_cap_x_
#define _Pre_opt_z_cap_x_(size)
#endif
#ifndef _Pre_ptrdiff_cap_
#define _Pre_ptrdiff_cap_(ptr)
#endif
#ifndef _Pre_ptrdiff_count_
#define _Pre_ptrdiff_count_(ptr)
#endif
#ifndef _Pre_readable_byte_size_
#define _Pre_readable_byte_size_(size)
#endif
#ifndef _Pre_readable_size_
#define _Pre_readable_size_(size)
#endif
#ifndef _Pre_readonly_
#define _Pre_readonly_
#endif
#ifndef _Pre_satisfies_
#define _Pre_satisfies_(cond)
#endif
#ifndef _Pre_unknown_
#define _Pre_unknown_
#endif
#ifndef _Pre_valid_
#define _Pre_valid_
#endif
#ifndef _Pre_valid_bytecap_
#define _Pre_valid_bytecap_(size)
#endif
#ifndef _Pre_valid_bytecap_c_
#define _Pre_valid_bytecap_c_(size)
#endif
#ifndef _Pre_valid_bytecap_x_
#define _Pre_valid_bytecap_x_(size)
#endif
#ifndef _Pre_valid_cap_
#define _Pre_valid_cap_(size)
#endif
#ifndef _Pre_valid_cap_c_
#define _Pre_valid_cap_c_(size)
#endif
#ifndef _Pre_valid_cap_x_
#define _Pre_valid_cap_x_(size)
#endif
#ifndef _Pre_writable_byte_size_
#define _Pre_writable_byte_size_(size)
#endif
#ifndef _Pre_writable_size_
#define _Pre_writable_size_(size)
#endif
#ifndef _Pre_writeonly_
#define _Pre_writeonly_
#endif
#ifndef _Pre_z_
#define _Pre_z_
#endif
#ifndef _Pre_z_bytecap_
#define _Pre_z_bytecap_(size)
#endif
#ifndef _Pre_z_bytecap_c_
#define _Pre_z_bytecap_c_(size)
#endif
#ifndef _Pre_z_bytecap_x_
#define _Pre_z_bytecap_x_(size)
#endif
#ifndef _Pre_z_cap_
#define _Pre_z_cap_(size)
#endif
#ifndef _Pre_z_cap_c_
#define _Pre_z_cap_c_(size)
#endif
#ifndef _Pre_z_cap_x_
#define _Pre_z_cap_x_(size)
#endif
#ifndef _Prepost_bytecount_
#define _Prepost_bytecount_(size)
#endif
#ifndef _Prepost_bytecount_c_
#define _Prepost_bytecount_c_(size)
#endif
#ifndef _Prepost_bytecount_x_
#define _Prepost_bytecount_x_(size)
#endif
#ifndef _Prepost_count_
#define _Prepost_count_(size)
#endif
#ifndef _Prepost_count_c_
#define _Prepost_count_c_(size)
#endif
#ifndef _Prepost_count_x_
#define _Prepost_count_x_(size)
#endif
#ifndef _Prepost_opt_bytecount_
#define _Prepost_opt_bytecount_(size)
#endif
#ifndef _Prepost_opt_bytecount_c_
#define _Prepost_opt_bytecount_c_(size)
#endif
#ifndef _Prepost_opt_bytecount_x_
#define _Prepost_opt_bytecount_x_(size)
#endif
#ifndef _Prepost_opt_count_
#define _Prepost_opt_count_(size)
#endif
#ifndef _Prepost_opt_count_c_
#define _Prepost_opt_count_c_(size)
#endif
#ifndef _Prepost_opt_count_x_
#define _Prepost_opt_count_x_(size)
#endif
#ifndef _Prepost_opt_valid_
#define _Prepost_opt_valid_
#endif
#ifndef _Prepost_opt_z_
#define _Prepost_opt_z_
#endif
#ifndef _Prepost_valid_
#define _Prepost_valid_
#endif
#ifndef _Prepost_z_
#define _Prepost_z_
#endif
#ifndef _Printf_format_string_
#define _Printf_format_string_
#endif
#ifndef _Printf_format_string_params_
#define _Printf_format_string_params_(x)
#endif
#ifndef _Raises_SEH_exception_
#define _Raises_SEH_exception_
#endif
#ifndef _Readable_bytes_
#define _Readable_bytes_(size)
#endif
#ifndef _Readable_elements_
#define _Readable_elements_(size)
#endif
#ifndef _Releases_exclusive_lock_
#define _Releases_exclusive_lock_(lock)
#endif
#ifndef _Releases_lock_
#define _Releases_lock_(lock)
#endif
#ifndef _Releases_nonreentrant_lock_
#define _Releases_nonreentrant_lock_(lock)
#endif
#ifndef _Releases_shared_lock_
#define _Releases_shared_lock_(lock)
#endif
#ifndef _Requires_exclusive_lock_held_
#define _Requires_exclusive_lock_held_(lock)
#endif
#ifndef _Requires_lock_held_
#define _Requires_lock_held_(lock)
#endif
#ifndef _Requires_lock_not_held_
#define _Requires_lock_not_held_(lock)
#endif
#ifndef _Requires_no_locks_held_
#define _Requires_no_locks_held_
#endif
#ifndef _Requires_shared_lock_held_
#define _Requires_shared_lock_held_(lock)
#endif
#ifndef _Reserved_
#define _Reserved_
#endif
#ifndef _Result_nullonfailure_
#define _Result_nullonfailure_
#endif
#ifndef _Result_zeroonfailure_
#define _Result_zeroonfailure_
#endif
#ifndef _Ret_
#define _Ret_
#endif
#ifndef _Ret_bound_
#define _Ret_bound_
#endif
#ifndef _Ret_bytecap_
#define _Ret_bytecap_(size)
#endif
#ifndef _Ret_bytecap_c_
#define _Ret_bytecap_c_(size)
#endif
#ifndef _Ret_bytecap_x_
#define _Ret_bytecap_x_(size)
#endif
#ifndef _Ret_bytecount_
#define _Ret_bytecount_(size)
#endif
#ifndef _Ret_bytecount_c_
#define _Ret_bytecount_c_(size)
#endif
#ifndef _Ret_bytecount_x_
#define _Ret_bytecount_x_(size)
#endif
#ifndef _Ret_cap_
#define _Ret_cap_(size)
#endif
#ifndef _Ret_cap_c_
#define _Ret_cap_c_(size)
#endif
#ifndef _Ret_cap_x_
#define _Ret_cap_x_(size)
#endif
#ifndef _Ret_count_
#define _Ret_count_(size)
#endif
#ifndef _Ret_count_c_
#define _Ret_count_c_(size)
#endif
#ifndef _Ret_count_x_
#define _Ret_count_x_(size)
#endif
#ifndef _Ret_maybenull_
#define _Ret_maybenull_
#endif
#ifndef _Ret_maybenull_z_
#define _Ret_maybenull_z_
#endif
#ifndef _Ret_notnull_
#define _Ret_notnull_
#endif
#ifndef _Ret_null_
#define _Ret_null_
#endif
#ifndef _Ret_opt_
#define _Ret_opt_
#endif
#ifndef _Ret_opt_bytecap_
#define _Ret_opt_bytecap_(size)
#endif
#ifndef _Ret_opt_bytecap_c_
#define _Ret_opt_bytecap_c_(size)
#endif
#ifndef _Ret_opt_bytecap_x_
#define _Ret_opt_bytecap_x_(size)
#endif
#ifndef _Ret_opt_bytecount_
#define _Ret_opt_bytecount_(size)
#endif
#ifndef _Ret_opt_bytecount_c_
#define _Ret_opt_bytecount_c_(size)
#endif
#ifndef _Ret_opt_bytecount_x_
#define _Ret_opt_bytecount_x_(size)
#endif
#ifndef _Ret_opt_cap_
#define _Ret_opt_cap_(size)
#endif
#ifndef _Ret_opt_cap_c_
#define _Ret_opt_cap_c_(size)
#endif
#ifndef _Ret_opt_cap_x_
#define _Ret_opt_cap_x_(size)
#endif
#ifndef _Ret_opt_count_
#define _Ret_opt_count_(size)
#endif
#ifndef _Ret_opt_count_c_
#define _Ret_opt_count_c_(size)
#endif
#ifndef _Ret_opt_count_x_
#define _Ret_opt_count_x_(size)
#endif
#ifndef _Ret_opt_valid_
#define _Ret_opt_valid_
#endif
#ifndef _Ret_opt_z_
#define _Ret_opt_z_
#endif
#ifndef _Ret_opt_z_bytecap_
#define _Ret_opt_z_bytecap_(size)
#endif
#ifndef _Ret_opt_z_bytecount_
#define _Ret_opt_z_bytecount_(size)
#endif
#ifndef _Ret_opt_z_cap_
#define _Ret_opt_z_cap_(size)
#endif
#ifndef _Ret_opt_z_count_
#define _Ret_opt_z_count_(size)
#endif
#ifndef _Ret_range_
#define _Ret_range_(lb,ub)
#endif
#ifndef _Ret_valid_
#define _Ret_valid_
#endif
#ifndef _Ret_writes_
#define _Ret_writes_(size)
#endif
#ifndef _Ret_writes_bytes_
#define _Ret_writes_bytes_(size)
#endif
#ifndef _Ret_writes_bytes_maybenull_
#define _Ret_writes_bytes_maybenull_(size)
#endif
#ifndef _Ret_writes_bytes_to_
#define _Ret_writes_bytes_to_(size,count)
#endif
#ifndef _Ret_writes_bytes_to_maybenull_
#define _Ret_writes_bytes_to_maybenull_(size,count)
#endif
#ifndef _Ret_writes_maybenull_
#define _Ret_writes_maybenull_(size)
#endif
#ifndef _Ret_writes_maybenull_z_
#define _Ret_writes_maybenull_z_(size)
#endif
#ifndef _Ret_writes_to_
#define _Ret_writes_to_(size,count)
#endif
#ifndef _Ret_writes_to_maybenull_
#define _Ret_writes_to_maybenull_(size,count)
#endif
#ifndef _Ret_writes_z_
#define _Ret_writes_z_(size)
#endif
#ifndef _Ret_z_
#define _Ret_z_
#endif
#ifndef _Ret_z_bytecap_
#define _Ret_z_bytecap_(size)
#endif
#ifndef _Ret_z_bytecount_
#define _Ret_z_bytecount_(size)
#endif
#ifndef _Ret_z_cap_
#define _Ret_z_cap_(size)
#endif
#ifndef _Ret_z_count_
#define _Ret_z_count_(size)
#endif
#ifndef _Return_type_success_
#define _Return_type_success_(expr)
#endif
#ifndef _Scanf_format_string_
#define _Scanf_format_string_
#endif
#ifndef _Scanf_format_string_params_
#define _Scanf_format_string_params_(x)
#endif
#ifndef _Scanf_s_format_string_
#define _Scanf_s_format_string_
#endif
#ifndef _Scanf_s_format_string_params_
#define _Scanf_s_format_string_params_(x)
#endif
#ifndef _Strict_type_match_
#define _Strict_type_match_
#endif
#ifndef _Struct_size_bytes_
#define _Struct_size_bytes_(size)
#endif
#ifndef _Success_
#define _Success_(expr)
#endif
#ifndef _Unchanged_
#define _Unchanged_(e)
#endif
#ifndef _Use_decl_annotations_
#define _Use_decl_annotations_
#endif
#ifndef _Valid_
#define _Valid_
#endif
#ifndef _When_
#define _When_(expr, annos)
#endif
#ifndef _Writable_bytes_
#define _Writable_bytes_(size)
#endif
#ifndef _Writable_elements_
#define _Writable_elements_(size)
#endif
#ifndef _Write_guarded_by_
#define _Write_guarded_by_(lock)
#endif

#endif /* OS_DECL_ATTRIBUTES_SAL_H */
