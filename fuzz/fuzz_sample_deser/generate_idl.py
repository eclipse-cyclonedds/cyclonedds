#!/usr/bin/env python

import sys
import os
from fuzz_tools.rand_idl.creator import generate_random_types, generate_random_idl
from fuzz_tools.rand_idl.value import generate_random_instance
from fuzz_tools.rand_idl.compile import compile_idl

def die(code, s):
    print(s, file=sys.stderr)
    sys.exit(code)

MODULE_NAME = "Fuzz"
CORPUS="fuzz_sample_deser_seed_corpus"
if __name__ == '__main__':
    if len(sys.argv) != 3:
        die(0, "Usage: {} <seed> <target_directory>")
    seed = int(sys.argv[1], 16)
    directory = sys.argv[2]
    if not os.path.isdir(directory):
        die(-1, f"{directory} is not a directory")

    # Generate random idl
    scope = generate_random_types(MODULE_NAME, number=25, seed=seed)
    idl_text = generate_random_idl(scope)
    with open(os.path.join(directory, "fuzz_sample.idl"), "w") as f:
        f.write(idl_text)

    # Hacky way to identify top-level types.
    toplvltypes = [e for e in scope.entities if getattr(e, "extensibility", False)]
    # Generate fuzz_samples.h, collecting all generated types
    with open(os.path.join(directory, "fuzz_samples.h"), "w") as f:
        f.write("#include \"fuzz_sample.h\"\n")
        f.write("static const char *idl_types_seed = \"{}\";".format(sys.argv[1]))
        f.write("static const struct dds_topic_descriptor *fixed_types[] = {\n")
        for entity in toplvltypes:
            entry = "&{}_{}_desc".format(MODULE_NAME, entity.name)
            if entity != scope.entities[-1]:
                f.write(f"\t{entry},\n")
            else:
                f.write(f"\t{entry}\n")
        f.write("};\n")

    # Generate initial corpus
    imported, tdir = compile_idl(idl_text, MODULE_NAME)
    corpus = os.path.join(directory, CORPUS)
    if not os.path.isdir(corpus):
        os.mkdir(corpus)
    for entity in toplvltypes:
        t = getattr(imported.__fuzzytypes, entity.name)
        sample = generate_random_instance(t, seed=seed)
        fname = "seed_{}".format(entity.name)
        with open(os.path.join(corpus, fname), "wb") as f:
            f.write(sample.serialize())
