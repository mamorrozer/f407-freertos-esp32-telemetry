# Third-party notices

The root [`LICENSE`](LICENSE) applies to project-authored code in this repository. It does not replace or relicense third-party components listed below. Preserve each component's source-file notices when copying or redistributing those files.

| Component | Version evidence in this tree | Location | License |
|---|---|---|---|
| FreeRTOS Kernel and GCC ARM_CM4F port | Kernel sources identify V10.3.1 | `stage2/F407_SensorTerminal/Middlewares/FreeRTOS_GCC/` | MIT; full text: [`LICENSES/FreeRTOS-MIT.txt`](LICENSES/FreeRTOS-MIT.txt). The same notice is present in upstream source headers. |
| STM32F4 HAL Driver | Version macros in `stm32f4xx_hal.c` identify V1.7.11 | `stage2/F407_SensorTerminal/Drivers/STM32F4xx_HAL_Driver/` | BSD 3-Clause; full text: [`LICENSES/BSD-3-Clause-ST.txt`](LICENSES/BSD-3-Clause-ST.txt). Copyright years in individual source files are preserved. |
| Arm CMSIS Core headers | Individual headers carry their own version comments and Apache-2.0 SPDX notices | `stage2/F407_SensorTerminal/Drivers/CMSIS/Include/` | Apache-2.0; full text: [`LICENSES/Apache-2.0.txt`](LICENSES/Apache-2.0.txt). Version comments differ between some headers, so no single package version is asserted here. |
| ST STM32F4 CMSIS Device, system and startup support | Device header identifies V2.6.6 | `stage2/F407_SensorTerminal/Drivers/CMSIS/Device/ST/STM32F4xx/` and `stage2/F407_SensorTerminal/startup_stm32f407xx_gcc.s` | BSD 3-Clause; full text: [`LICENSES/BSD-3-Clause-ST.txt`](LICENSES/BSD-3-Clause-ST.txt). |

The ESP-IDF framework and its built-in components are installed separately and are not vendored in this repository. Use ESP-IDF v5.4 as documented in [`docs/BUILD_AND_FLASH.md`](docs/BUILD_AND_FLASH.md), and consult the licenses shipped with that framework installation.

## Upstream references

- [FreeRTOS Kernel](https://github.com/FreeRTOS/FreeRTOS-Kernel) — license and copyright notices are also retained in the vendored source files.
- [STM32CubeF4](https://github.com/STMicroelectronics/STM32CubeF4) — ST HAL and device support source files identify ST's BSD 3-Clause terms and retain their copyright notices.
- [CMSIS](https://github.com/ARM-software/CMSIS_5) — Arm CMSIS Core headers identify Apache-2.0.

This notice documents the license identifiers and version strings found in the repository. It is not a claim that the vendored files are byte-for-byte identical to a particular upstream package release.
