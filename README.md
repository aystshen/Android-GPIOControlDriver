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
	&i2c2 {  
		status = "okay";
		topband_gpio: topband_gpio { 
			status = "okay";
			compatible = "topband,gpio";
			
			topband,gpios {
				topband,gpio0 {
					topband,gpio = <&gpio0 GPIO_B6 GPIO_ACTIVE_HIGH>;
					topband,config = <2>; // 0: output(LOW) 1: output(HIGH) 2: input
				};
				topband,gpio1 {
					topband,gpio = <&gpio0 GPIO_B5 GPIO_ACTIVE_HIGH>;
					topband,config = <2>;
				};
				topband,gpio2 {
					topband,gpio = <&gpio0 GPIO_B1 GPIO_ACTIVE_HIGH>;
					topband,config = <2>;
				};
				topband,gpio3 {
					topband,gpio = <&gpio0 GPIO_B0 GPIO_ACTIVE_HIGH>;
					topband,config = <2>;
				};
				topband,gpio4 {
					topband,gpio = <&gpio0 GPIO_D1 GPIO_ACTIVE_HIGH>;
					topband,config = <2>;
				};
				topband,gpio5 {
					topband,gpio = <&gpio1 GPIO_A3 GPIO_ACTIVE_HIGH>;
					topband,config = <2>;
				};
				topband,gpio6 {
					topband,gpio = <&gpio1 GPIO_B3 GPIO_ACTIVE_HIGH>;
					topband,config = <2>;
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
	Copyright 2019 Bob Shen.

	Licensed under the Apache License, Version 2.0 (the "License");
	you may not use this file except in compliance with the License.
	You may obtain a copy of the License at

	http://www.apache.org/licenses/LICENSE-2.0

	Unless required by applicable law or agreed to in writing, software
	distributed under the License is distributed on an "AS IS" BASIS,
	WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
	See the License for the specific language governing permissions and
	limitations under the License.
