#!/usr/bin/env python

import argparse
import os
from fuzz_tools.rand_idl.creator import generate_random_types, generate_random_idl
from fuzz_tools.rand_idl.value import generate_random_instance
from fuzz_tools.rand_idl.compile import compile_idl

MODULE_NAME = "Fuzz"


def parse_args():
    parser = argparse.ArgumentParser()
    parser.add_argument("--xcdr-version", type=int, choices=(1, 2), required=True)
    parser.add_argument("--corpus-name", required=True)
    parser.add_argument("seed")
    parser.add_argument("target_directory")
    return parser.parse_args()


if __name__ == '__main__':
    args = parse_args()
    seed = int(args.seed, 16)
    directory = args.target_directory
    os.makedirs(directory, exist_ok=True)

    # Generate random idl
    scope = generate_random_types(MODULE_NAME, xcdr_version=args.xcdr_version, number=25, seed=seed)
    idl_text = generate_random_idl(scope)
    with open(os.path.join(directory, "fuzz_sample.idl"), "w") as f:
        f.write(idl_text)

    # Hacky way to identify top-level types.
    toplvltypes = [e for e in scope.entities if getattr(e, "extensibility", False)]
    # Generate fuzz_samples.h, collecting all generated types
    with open(os.path.join(directory, "fuzz_samples.h"), "w") as f:
        f.write("#include <stdint.h>\n")
        f.write("#include \"fuzz_sample.h\"\n")
        f.write("static const char *idl_types_seed = \"{}\";\n".format(args.seed))
        f.write("static const uint32_t idl_types_xcdr_version = {};\n".format(args.xcdr_version))
        f.write("static const struct dds_topic_descriptor *fixed_types[] = {\n")
        for idx, entity in enumerate(toplvltypes):
            entry = "&{}_{}_desc".format(MODULE_NAME, entity.name)
            comma = "," if idx + 1 < len(toplvltypes) else ""
            f.write(f"\t{entry}{comma}\n")
        f.write("};\n")

    # Generate initial corpus
    imported, tdir = compile_idl(idl_text, MODULE_NAME)
    corpus = os.path.join(directory, args.corpus_name)
    os.makedirs(corpus, exist_ok=True)
    for fname in os.listdir(corpus):
        if fname.startswith("seed_"):
            os.unlink(os.path.join(corpus, fname))
    for entity in toplvltypes:
        t = getattr(imported.__fuzzytypes, entity.name)
        sample = generate_random_instance(t, seed=seed)
        fname = "seed_{}".format(entity.name)
        with open(os.path.join(corpus, fname), "wb") as f:
            f.write(sample.serialize())
