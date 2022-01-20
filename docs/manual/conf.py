from pathlib import Path

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


html_static_path = ['_static']

autosummary_generate=True
# Setup the exhale extension
exhale_args = {
    "containmentFolder":     str(Path("@_sourcedir@/ddsc_api_docs").resolve()),
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
