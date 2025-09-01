[![PyPI](https://img.shields.io/pypi/v/mqt.syrec?logo=pypi&style=flat-square)](https://pypi.org/project/mqt.syrec/)
![OS](https://img.shields.io/badge/os-linux%20%7C%20macos%20%7C%20windows-blue?style=flat-square)
[![License: MIT](https://img.shields.io/badge/license-MIT-blue.svg?style=flat-square)](https://opensource.org/licenses/MIT)
[![CI](https://img.shields.io/github/actions/workflow/status/munich-quantum-toolkit/syrec/ci.yml?branch=main&style=flat-square&logo=github&label=ci)](https://github.com/munich-quantum-toolkit/syrec/actions/workflows/ci.yml)
[![CD](https://img.shields.io/github/actions/workflow/status/munich-quantum-toolkit/syrec/cd.yml?style=flat-square&logo=github&label=cd)](https://github.com/munich-quantum-toolkit/syrec/actions/workflows/cd.yml)
[![codecov](https://img.shields.io/codecov/c/github/munich-quantum-toolkit/syrec?style=flat-square&logo=codecov)](https://codecov.io/gh/munich-quantum-toolkit/syrec)

> [!NOTE]
> This project is currently in low maintenance mode.
> We will still fix bugs and accept pull requests, but we will not actively develop new features.

<p align="center">
  <a href="https://mqt.readthedocs.io">
    <picture>
      <source media="(prefers-color-scheme: dark)" srcset="https://raw.githubusercontent.com/munich-quantum-toolkit/.github/refs/heads/main/docs/_static/logo-mqt-dark.svg" width="60%">
      <img src="https://raw.githubusercontent.com/munich-quantum-toolkit/.github/refs/heads/main/docs/_static/logo-mqt-light.svg" width="60%" alt="MQT Logo">
    </picture>
  </a>
</p>

# MQT SyReC - A Tool for HDL-based Synthesis of Reversible Circuits

MQT SyReC is a tool for HDL-based synthesis of reversible circuits.
It is part of the [_Munich Quantum Toolkit (MQT)_](https://mqt.readthedocs.io).

<p align="center">
  <a href="https://mqt.readthedocs.io/projects/syrec">
  <img width=30% src="https://img.shields.io/badge/documentation-blue?style=for-the-badge&logo=read%20the%20docs" alt="Documentation" />
  </a>
</p>

## Key Features

- **Automatic synthesis of reversible circuits from high-level HDL**: Accepts any HDL description following the [SyReC grammar and syntax](https://mqt.readthedocs.io/projects/syrec/en/latest/SyrecLanguageSemantics.html), enabling rapid prototyping and design of reversible logic.
- **Two complementary synthesis schemes**: Choose between [cost-aware synthesis](https://mqt.readthedocs.io/projects/syrec/en/latest/DescriptionAndFeatures.html#cost-aware-synthesis) (minimizing gate cost) and [line-aware synthesis](https://mqt.readthedocs.io/projects/syrec/en/latest/DescriptionAndFeatures.html#line-aware-synthesis) (minimizing circuit lines), each with distinct trade-offs for resource optimization.
- **Graphical User Interface (GUI)**: Intuitive GUI for specifying SyReC programs, visualizing circuits, and running synthesis, simulation, and cost analysis at the click of a button.
- **Simulation and cost analysis**: Simulate synthesized circuits and determine gate costs directly within the tool.
- **Comprehensive SyReC language support**: Implements the full SyReC language, including modules, parameterized bitwidths, multi-dimensional variables, and advanced assignment semantics ([language reference](https://mqt.readthedocs.io/projects/syrec/en/latest/SyrecLanguageSemantics.html)).
- **Python API and C++ core**: High-performance C++ backend with Python bindings for integration into research and teaching workflows.
- **Cross-platform and easy to install**: Prebuilt Python wheels for Linux, macOS, and Windows via [PyPI](https://pypi.org/project/mqt.syrec/).

If you have any questions, feel free to create a [discussion](https://github.com/munich-quantum-toolkit/syrec/discussions) or an [issue](https://github.com/munich-quantum-toolkit/syrec/issues) on [GitHub](https://github.com/munich-quantum-toolkit/syrec).

## Contributors and Supporters

The _[Munich Quantum Toolkit (MQT)](https://mqt.readthedocs.io)_ is developed by the [Chair for Design Automation](https://www.cda.cit.tum.de/) at the [Technical University of Munich](https://www.tum.de/) and supported by the [Munich Quantum Software Company (MQSC)](https://munichquantum.software).
Among others, it is part of the [Munich Quantum Software Stack (MQSS)](https://www.munich-quantum-valley.de/research/research-areas/mqss) ecosystem, which is being developed as part of the [Munich Quantum Valley (MQV)](https://www.munich-quantum-valley.de) initiative.

<p align="center">
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset="https://raw.githubusercontent.com/munich-quantum-toolkit/.github/refs/heads/main/docs/_static/mqt-logo-banner-dark.svg" width="90%">
    <img src="https://raw.githubusercontent.com/munich-quantum-toolkit/.github/refs/heads/main/docs/_static/mqt-logo-banner-light.svg" width="90%" alt="MQT Partner Logos">
  </picture>
</p>

Thank you to all the contributors who have helped make MQT SyReC a reality!

<p align="center">
  <a href="https://github.com/munich-quantum-toolkit/syrec/graphs/contributors">
  <img src="https://contrib.rocks/image?repo=munich-quantum-toolkit/syrec" alt="Contributors to munich-quantum-toolkit/syrec" />
  </a>
</p>

The MQT will remain free, open-source, and permissively licensed—now and in the future.
We are firmly committed to keeping it open and actively maintained for the quantum computing community.

To support this endeavor, please consider:

- Starring and sharing our repositories: https://github.com/munich-quantum-toolkit
- Contributing code, documentation, tests, or examples via issues and pull requests
- Citing the MQT in your publications (see [Cite This](#cite-this))
- Citing our research in your publications (see [References](https://mqt.readthedocs.io/projects/syrec/en/latest/references.html))
- Using the MQT in research and teaching, and sharing feedback and use cases
- Sponsoring us on GitHub: https://github.com/sponsors/munich-quantum-toolkit

<p align="center">
  <a href="https://github.com/sponsors/munich-quantum-toolkit">
  <img width=20% src="https://img.shields.io/badge/Sponsor-white?style=for-the-badge&logo=githubsponsors&labelColor=black&color=blue" alt="Sponsor the MQT" />
  </a>
</p>

## Getting Started

`mqt.syrec` is available via [PyPI](https://pypi.org/project/mqt.syrec/).

```console
(.venv) $ pip install mqt.syrec
```

Once installed, start the MQT SyReC GUI by running:

```console
(.venv) $ syrec-editor
```

**Detailed documentation and examples are available at [ReadTheDocs](https://mqt.readthedocs.io/projects/syrec).**

## System Requirements

Building the project requires a C++ compiler with support for C++20 and CMake 3.24 or newer.
For detail on how to build the project, please refer to the [documentation](https://mqt.readthedocs.io/projects/syrec).
Building (and running) is continuously tested under Linux, macOS, and Windows using the [latest available system versions for GitHub Actions](https://github.com/actions/runner-images).
MQT SyReC is compatible with all supported Python versions.

## Cite This

Please cite the work that best fits your use case.

### MQT SyReC (the tool)

When citing the software itself or results produced with it, cite the MQT SyReC paper:

```bibtex
@article{adarsh2022syrecSynthesizer,
  title        = {{SyReC} {Synthesizer}: {An} {MQT} tool for synthesis of reversible circuits},
  author       = {Adarsh, Smaran and Burgholzer, Lukas and Manjunath, Tanmay and Wille, Robert},
  year         = {2022},
  journal      = {Software Impacts},
  publisher    = {Elsevier},
  url          = {https://doi.org/10.1016/j.simpa.2022.100451}
}
```

### The Munich Quantum Toolkit (the project)

When discussing the overall MQT project or its ecosystem, cite the MQT Handbook:

```bibtex
@inproceedings{mqt,
  title        = {The {{MQT}} Handbook: {{A}} Summary of Design Automation Tools and Software for Quantum Computing},
  shorttitle   = {{The MQT Handbook}},
  author       = {Wille, Robert and Berent, Lucas and Forster, Tobias and Kunasaikaran, Jagatheesan and Mato, Kevin and Peham, Tom and Quetschlich, Nils and Rovara, Damian and Sander, Aaron and Schmid, Ludwig and Schoenberger, Daniel and Stade, Yannick and Burgholzer, Lukas},
  year         = 2024,
  booktitle    = {IEEE International Conference on Quantum Software (QSW)},
  doi          = {10.1109/QSW62656.2024.00013},
  eprint       = {2405.17543},
  eprinttype   = {arxiv},
  addendum     = {A live version of this document is available at \url{https://mqt.readthedocs.io}}
}
```

### Peer-Reviewed Research

When citing the underlying methods and research, please reference the most relevant peer-reviewed publications from the list below:

[[1]](https://doi.org/10.1016/j.simpa.2022.100451)
S. Adarsh, L. Burgholzer, T. Manjunath, and R. Wille.
SyReC Synthesizer: An MQT tool for synthesis of reversible circuits.
_Software Impacts_, 2022.

[[2]](http://www.informatik.uni-bremen.de/agra/doc/konf/10_syrec_reversible_hardware_language.pdf)
R. Wille, S. Offermann, and R. Drechsler.
SyReC: A Programming Language for Synthesis of Reversible Circuits.
_Forum on Specification and Design Languages (FDL)_, 2010.

[[3]](https://doi.org/10.1016/j.vlsi.2015.10.001)
R. Wille, E. Schönborn, M. Soeken, and R. Drechsler.
SyReC: A hardware description language for the specification and synthesis of reversible circuits.
_Integration (The VLSI Journal)_, 2016.

[[4]](https://www.cda.cit.tum.de/files/eda/2019_iccad_hdl_based_reversible_circuit_synthesis_without_additional_lines.pdf)
R. Wille, M. Haghparast, S. Adarsh, and T. Manjunath.
Towards HDL-based Synthesis of Reversible Circuits with No Additional Lines.
_International Conference on Computer Aided Design (ICCAD)_, 2019.

---

## Acknowledgements

The Munich Quantum Toolkit has been supported by the European Research Council (ERC) under the European Union's Horizon 2020 research and innovation program (grant agreement No. 101001318), the Bavarian State Ministry for Science and Arts through the Distinguished Professorship Program, as well as the Munich Quantum Valley, which is supported by the Bavarian state government with funds from the Hightech Agenda Bayern Plus.

<p align="center">
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset="https://raw.githubusercontent.com/munich-quantum-toolkit/.github/refs/heads/main/docs/_static/mqt-funding-footer-dark.svg" width="90%">
    <img src="https://raw.githubusercontent.com/munich-quantum-toolkit/.github/refs/heads/main/docs/_static/mqt-funding-footer-light.svg" width="90%" alt="MQT Funding Footer">
  </picture>
</p>
