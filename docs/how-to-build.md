# How to build the documentation

For most purposes you can just use the documentation hosted on [cyclonedds.io](https://cyclonedds.io). However, if you do want to build the documentation yourself here is some info.

## Prerequisites

A Python installation version 3.7 or higher.

## Building

A virtualenv is recommended, this will isolate the documentation building dependencies from your user and system python.

```sh
$ python -m venv .venv
$ source .venv/bin/activate
```

Now install the dependencies:

```sh
$ pip install -r requirements.txt
```

You can now build the documentation:

```sh
$ sphinx-build ./manual ./output
```

You can host the documentation with a quick python command to view it in your browser easily:

```sh
$ python -m http.server --directory output
```

When rebuilding the docs in a new shell you can skip the installation of the dependencies and simply activate the virtual environment:

```sh
$ source .venv/bin/activate
$ sphinx-build ./manual ./output
```
