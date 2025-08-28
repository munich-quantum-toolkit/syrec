Installation
============

*SyReC Synthesizer* is mainly developed as a C++ library.
In order to make the tool as accessible as possible, it comes with an easy-to-use Python interface.

We encourage installing *SyReC Synthesizer* via pip (preferably in a `virtual environment <https://docs.python.org/3/library/venv.html>`_):

    .. code-block:: console

        (venv) $ pip install mqt.syrec

Once installed, start the *SyReC Synthesizer* editor GUI by running:

    .. code-block:: console

        (venv) $ syrec-editor

In most practical cases (under 64-bit Linux, MacOS incl. Apple Silicon, and Windows), this requires no compilation and merely downloads and installs a platform-specific pre-built wheel.

.. note::
    In order to set up a virtual environment, you can use the following commands:

    .. code-block:: console

        $ python3 -m venv venv
        $ source venv/bin/activate

    If you are using Windows, you can use the following commands instead:

    .. code-block:: console

        $ python3 -m venv venv
        $ venv\Scripts\activate.bat

    It is recommended to make sure that you are using the latest version of pip, setuptools, and wheel before trying to install the project:

    .. code-block:: console

        (venv) $ pip install --upgrade pip setuptools wheel

Building from Source for Performance
####################################

In order to get the best performance out of *SyReC Synthesizer* and enable platform-specific compiler optimizations that cannot be enabled on portable wheels, it is recommended to build the package from source via:

    .. code-block:: console

        (venv) $ pip install mqt.syrec --no-binary mqt.syrec

This requires a `C++ compiler <https://en.wikipedia.org/wiki/List_of_compilers#C++_compilers>`_ compiler supporting *C++17*, a minimum `CMake <https://cmake.org/>`_ version of *3.24*.

The library is continuously tested under Linux, MacOS, and Windows using the `latest available system versions for GitHub Actions <https://github.com/actions/virtual-environments>`_.
In order to access the latest build logs, visit `syrec/actions/workflows/ci.yml <https://github.com/cda-tum/syrec/actions/workflows/ci.yml>`_.

.. note::
    We noticed some issues when compiling with Microsoft's *MSCV* compiler toolchain. If you want to start development on this project under Windows, consider using the *clang* compiler toolchain. A detailed description of how to set this up can be found `here <https://docs.microsoft.com/en-us/cpp/build/clang-support-msbuild?view=msvc-160>`_.
