# Android GPIO Control
This is used to implement the Android APP to control the CPU GPIO, and supports setting the IO direction, IO output, IO input, and key.

## Usage
1. Copy topband_gpio to kernel/drivers/misc/ directory.
2. Modify the kernel/drivers/misc/Kconfig as follows:
```
source "drivers/misc/topband_gpio/Kconfig"
```
3. Modify the kernel/drivers/misc/Makefile as follows:
```
obj-$(CONFIG_TOPBAND_GPIO_DRIVER) += topband_gpio/
```
4. Modify the dts as follows:
```
/ {  
	topband_gpio: topband_gpio { 
		status = "okay";
		compatible = "topband,gpio";

		topband,gpios {
			topband,gpio0 {
				topband,gpio = <&gpio0 GPIO_B6 GPIO_ACTIVE_HIGH>;
			};
			topband,gpio1 {
				topband,gpio = <&gpio0 GPIO_B5 GPIO_ACTIVE_HIGH>;
			};
			topband,gpio2 {
				topband,gpio = <&gpio0 GPIO_B1 GPIO_ACTIVE_HIGH>;
			};
			topband,gpio3 {
				topband,gpio = <&gpio0 GPIO_B0 GPIO_ACTIVE_HIGH>;
			};
			topband,gpio4 {
				topband,gpio = <&gpio0 GPIO_D1 GPIO_ACTIVE_HIGH>;
			};
			topband,gpio5 {
				topband,gpio = <&gpio1 GPIO_A3 GPIO_ACTIVE_HIGH>;
			};
			topband,gpio6 {
				topband,gpio = <&gpio1 GPIO_B3 GPIO_ACTIVE_HIGH>;
			};
		};
	};
};
```
5. Marge gpio.patch.

## API
[API description](./gpio_api_usage.md)
[Download demo](https://fir.im/1a4h)

## Developed By
* ayst.shen@foxmail.com

## License
	Copyright  (C)  2016 - 2017 Topband. Ltd.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be a reference
    to you, when you are integrating the android gpio into your system,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    General Public License for more details.
