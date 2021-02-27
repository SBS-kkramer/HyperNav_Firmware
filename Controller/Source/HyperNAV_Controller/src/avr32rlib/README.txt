Satlantic AVR32 Resources Library (avr32rlib)
---------------------------------------------

Author(s): 

Diego Sorrentino, Satlantic Inc.

History:

2011-02-08, DAS, First release.

===================================================================================================================


Overview:
--------

The Satlantic AVR32 Resources Library (avr32rlib) is composed of several reusable modules that can be independently
added to AVR32 projects.

Currently (as of Jan-2011), the library components are divided into two main categories:

- Utilities (./Utils/). Higher level utilites such as: Buffers, File Handling, System Time Management, etc.
- Components (./Components/). Low level drivers for a diversity of discrete and complex hardware components.


Dependencies:
------------

The Satlantic AVR32 Resources Library is built over the 'Atmel AVR32 Software Framework' and the 'AVR32 newlib'.
In addition, some library modules may depend on other library sub-components.


Use:
---

Each module is typically composed of three files,

- moduleName.h (API)
- moduleName.c (Implementation)
- moduleName_cfg_template.h (User editable module configuration file template)

The 'moduleName.h' files describe the module programming interface (API) while the 'moduleName.c' files implement
the documented API. Configuration file templates ('moduleName_cfg_template.h') must be appropriately modified
by the library user to accomodate for the target hardware, renamed to 'moduleName_cfg.h', and placed in the library
configuration folder (./Config/).

The library user is free to include any portion of the library code in its project as needed.


Variants:
---------

To maintain compatibility with several platforms the library can be compiled with different behavioral variants. 
The idea is that the library behavior could be 'fine-tuned' to fit different platform needs. 

The variants are selected by assigning a value to the constant 'AVR32RLIB_VARIANT' at compile time. For example using the 
gcc '-D' option: gcc ... -DAVR32RLIB_VARIANT=AVR32RLIB_DEFAULT_VAR.

Available variants:

- AVR32RLIB_DEFAULT_VAR: Default variant


IMPORTANT NOTE: Hardware dependencies, like pin mappings, should be handled by appropriately setting up the corresponding 
configuration files (xyz_cfg.h). The use of behavioral variants should be kept at a minimum.

 

  



