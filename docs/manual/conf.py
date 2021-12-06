# -- Helper function ---------------------------------------------------------

# This ridiculous way of defining a function is to allow Sphinx to pickle this
# That it doesn't work by just defining the function is probably a bug in Sphinx.

from tempfile import TemporaryDirectory
import sys
import textwrap

with TemporaryDirectory() as d:
    with open(f'{d}/helper.py', 'w') as f:
        f.write(textwrap.dedent("""
            from docutils.nodes import make_id

            def my_make_id(string):
                # Github doesn't count '/' as a title separator, myst does.
                if not string.startswith('//'):
                    return make_id(string)
                return make_id(string.translate({ord(i): None for i in '[]/@'}).lower())
            """
        ))
    sys.path.append(d)
    from helper import my_make_id
    sys.path.pop()

# -- Project information -----------------------------------------------------

project = '@PROJECT_NAME@'
copyright = '2021, @PROJECT_NAME@ committers'
author = '@PROJECT_NAME@ committers'

# The short X.Y version
version = '@PROJECT_VERSION_MAJOR@.@PROJECT_VERSION_MINOR@.@PROJECT_VERSION_PATCH@'

# The full version, including alpha/beta/rc tags
release = '@PROJECT_VERSION@'


# -- General configuration ---------------------------------------------------

extensions = [
    'breathe',
    "exhale",
    'sphinx.ext.intersphinx',
    "sphinx.ext.viewcode",
    "sphinx.ext.autosummary"
]

templates_path = ['_templates']
language = 'en'
exclude_patterns = ['_build', 'Thumbs.db', '.DS_Store']


# -- Options for HTML output -------------------------------------------------

try:
    import sphinx_rtd_theme
    extensions.append('sphinx_rtd_theme')
    html_theme = 'sphinx_rtd_theme'
except ImportError:
    import warnings
    warnings.warn("The Sphinx rtd theme is not installed. Falling back to alabaster.")
    html_theme = 'alabaster'

# -- Options for Markdown inclusion -----------------------------------------

try:
    import myst_parser
    extensions.append('myst_parser')
    myst_heading_slug_func = my_make_id
    myst_heading_anchors = 10
except ImportError:
    import warnings
    warnings.warn("The myst_parser is not installed, will not be including markdown documents.")


html_static_path = ['_static']

autosummary_generate=True
# Setup the exhale extension
exhale_args = {
    "containmentFolder":     "@_sourcedir@/ddsc_api_docs",
    "rootFileName":          "library_root.rst",
    "rootFileTitle":         "Raw C API",
    "fullToctreeMaxDepth":   1,
    "createTreeView":        True,
    "exhaleExecutesDoxygen": False,
    "doxygenStripFromPath":  ".",
}

# Tell sphinx what the primary language being documented is.
primary_domain = 'c'

# Tell sphinx what the pygments highlight language should be.
highlight_language = 'c'
