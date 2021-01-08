# CycloneDDS Getting Started Documentation

This directory contains the Getting Started document for Cyclone DDS. There are two parts, you can find the document in [markdown](./CycloneGettingStarted.md), or in the `CycloneDDS Getting Started Book` which is built with [mdbook](https://github.com/rust-lang/mdBook).

### Building the book

Building the book requires [mdbook](https://github.com/rust-lang/mdBook). To get it:

```
$ cargo install --git https://github.com/rust-lang/mdBook.git mdbook
```

This requires installing the `cargo`. If cargo is not installed, `sudo apt-get install cargo` should do the trick.

To build the book:

```
$ mdbook build
```

By executing this command, the [book](book/index.html) will be rendered. The SUMMARY.md file is used to understand the structure of your book, therefore all the files must be linked in the SUMMARY.md file.
