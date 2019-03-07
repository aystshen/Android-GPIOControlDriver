# GPIO API 调用方法
> 支持设置GPIO输入输出方向  
> 支持输出  
> 支持输入  
> 支持将GPIO设置为按键

### 方法1 依赖Framework jar包（不推荐）
1. 引入Android定制后的Framework jar包，out/target/common/obj/JAVA_LIBRARIES/framework_intermediates/classes.jar  
    1. 将classes.jar拷贝到app/libs/目录下
    2. 修改项目根目录build.gradle，在allprojects段内增加下面代码：  
    ```
    allprojects {
        repositories {
            google()
            jcenter()
        }
    
        gradle.projectsEvaluated {
            tasks.withType(JavaCompile) {
                options.compilerArgs.add('-Xbootclasspath/p:app\\libs\\classes.jar')
            }
        }
    }
    ```
    3. 修改app/build.gradle，如下：
    ```
    android{
        defaultConfig {
            multiDexEnabled true
        }
    }
    
    dependencies {
        compileOnly files('libs/classes.jar')
    }

    preBuild {
        doLast {
            def imlFile = file(project.name + ".iml")
            println('Change ' + project.name + '.iml order')
            try {
                def parsedXml = (new XmlParser()).parse(imlFile)
                def jdkNode = parsedXml.component[1].orderEntry.find { it.'@type' == 'jdk' }
                parsedXml.component[1].remove(jdkNode)
                def sdkString = "Android API " + android.compileSdkVersion.substring("android-".length()) + " Platform"
                new groovy.util.Node(parsedXml.component[1], 'orderEntry', ['type': 'jdk', 'jdkName': sdkString, 'jdkType': 'Android SDK'])
                groovy.xml.XmlUtil.serialize(parsedXml, new FileOutputStream(imlFile))
            } catch (FileNotFoundException e) {
                // nop, iml not found
            }
        }
    }
    ```
2. 调用
```
 SystemGpio gpioService = (SystemGpio) context.getSystemService("gpio");
```

### 方法二 反射调用（==推荐==）
1. 在APP源码src/main目录下新建aidl/android/os/这样的目录结构
2. 在aidl/android/os/目录下新建IGpioService.aidl文件，内容如下：
```
package android.os;
/** {@hide} */
interface IGpioService
{
	int gpioWrite(int gpio, int value);
	int gpioRead(int gpio);
	int gpioDirection(int gpio, int direction, int value);
	int gpioRegKeyEvent(int gpio);
	int gpioUnregKeyEvent(int gpio);
	int gpioGetNumber();
}
```
3. 反射调用
```
Method method = null;
try {
    method = Class.forName("android.os.ServiceManager").getMethod("getService", String.class);
    IBinder binder = (IBinder) method.invoke(null, new Object[]{"gpio"});
    IGpioService gpioService = IGpioService.Stub.asInterface(binder);
} catch (Exception e) {
    e.printStackTrace();
}
```

### API使用说明
#### 获取GPIO数量
在使用GPIO前建议先获取GPIO数量，当调用其它方法需要传入参数“gpio”时可以使用0~Number之间的值。  
如：gpioGetNumber()返回7，说明一共有7个GPIO，那么传入参数可以为：0、1、2、3、4、5、6。
```
/**
 * Get GPIO number
 * @return <0: error other: GPIO number
 */
public int gpioGetNumber() {
    if (null != mGpioService) {
        return mGpioService.gpioGetNumber();
    }
    return -1;
}
```

#### 设置输入输出模式
```
/**
 * GPIO direction
 * @param gpio 0~Number
 * @param direction 0: input 1: output
 * @param value 0: Low 1: High
 */
public void gpioDirection(int gpio, int direction, int value) {
    if (null != mGpioService) {
        mGpioService.gpioDirection(gpio, direction, value);
    }
}
```

#### 控制GPIO输出
```
/**
 * GPIO write
 * @param gpio 0~Number
 * @param value 0: Low 1: High
 */
public void gpioWrite(int gpio, int value) {
    if (null != mGpioService) {
        mGpioService.gpioWrite(gpio, value);
    }
}
```

#### 控制GPIO输入
```
/**
 * GPIO read
 * @param gpio 0~Number
 * @return 0: Low 1: High other：error
 */
public int gpioRead(int gpio) {
    if (null != mGpioService) {
        return mGpioService.gpioRead(gpio);
    }
    return -1;
}
```

#### 设置GPIO为按键模式
```
 /**
 * GPIO register key event
 * @param gpio 0~Number
 */
public void gpioRegKeyEvent(int gpio) {
    if (null != mGpioService) {
        mGpioService.gpioRegKeyEvent(gpio);
    }
}
```
设置为按键模式后，当GPIO有电平翻转时会上报按键事件，GPIO与KeyCode对应关系如下：
```
GPIO_0            KeyEvent.KEYCODE_GPIO_0
GPIO_1            KeyEvent.KEYCODE_GPIO_1
GPIO_2            KeyEvent.KEYCODE_GPIO_2
GPIO_3            KeyEvent.KEYCODE_GPIO_3
GPIO_4            KeyEvent.KEYCODE_GPIO_4
GPIO_5            KeyEvent.KEYCODE_GPIO_5
GPIO_6            KeyEvent.KEYCODE_GPIO_6
GPIO_7            KeyEvent.KEYCODE_GPIO_7
GPIO_8            KeyEvent.KEYCODE_GPIO_8
GPIO_9            KeyEvent.KEYCODE_GPIO_9
```
- 当GPIO变为低电平将上报：KeyEvent.ACTION_DOWN  
- 当GPIO变为高电平将上报：KeyEvent.ACTION_UP

如果要取消按键模式则调用如下方法：
```
/**
 * GPIO unregister key event
 * @param gpio 0~Number
 */
public void gpioUnregKeyEvent(int gpio) {
    if (null != mGpioService) {
        mGpioService.gpioUnregKeyEvent(gpio);
    }
}
```

### 例
#### 依赖Framework jar方式参考源码：
```
package com.ayst.item;

import android.annotation.SuppressLint;
import android.content.Context;
import android.os.SystemGpio;

/**
 * Created by Administrator on 2018/11/6.
 */

public class GpioTest {
    private SystemGpio mGpioService;

    @SuppressLint("WrongConstant")
    public GpioTest(Context context) {
        mGpioService = (SystemGpio) context.getSystemService("gpio");
    }

    /**
     * GPIO write
     * @param gpio 0~Number
     * @param value 0: Low 1: High
     */
    public void gpioWrite(int gpio, int value) {
        if (null != mGpioService) {
            mGpioService.gpioWrite(gpio, value);
        }
    }

    /**
     * GPIO read
     * @param gpio 0~Number
     * @return 0: Low 1: High other：error
     */
    public int gpioRead(int gpio) {
        if (null != mGpioService) {
            return mGpioService.gpioRead(gpio);
        }
        return -1;
    }

    /**
     * GPIO direction
     * @param gpio 0~Number
     * @param direction 0: input 1: output
     * @param value 0: Low 1: High
     */
    public void gpioDirection(int gpio, int direction, int value) {
        if (null != mGpioService) {
            mGpioService.gpioDirection(gpio, direction, value);
        }
    }

    /**
     * GPIO register key event
     * @param gpio 0~Number
     */
    public void gpioRegKeyEvent(int gpio) {
        if (null != mGpioService) {
            mGpioService.gpioRegKeyEvent(gpio);
        }
    }

    /**
     * GPIO unregister key event
     * @param gpio 0~Number
     */
    public void gpioUnregKeyEvent(int gpio) {
        if (null != mGpioService) {
            mGpioService.gpioUnregKeyEvent(gpio);
        }
    }

    /**
     * Get GPIO number
     * @return <0: error other: GPIO number
     */
    public int gpioGetNumber() {
        if (null != mGpioService) {
            return mGpioService.gpioGetNumber();
        }
        return -1;
    }
}

```

#### 反射方式参考源码：
```
package com.ayst.item;

import android.annotation.SuppressLint;
import android.content.Context;
import android.os.IBinder;
import android.os.IGpioService;
import android.os.RemoteException;

import java.lang.reflect.Method;

/**
 * Created by Administrator on 2018/11/6.
 */

public class GpioTest {
    private IGpioService mGpioService;

    @SuppressLint("WrongConstant")
    public GpioTest(Context context) {
        Method method = null;
        try {
            method = Class.forName("android.os.ServiceManager").getMethod("getService", String.class);
            IBinder binder = (IBinder) method.invoke(null, new Object[]{"gpio"});
            mGpioService = IGpioService.Stub.asInterface(binder);
        } catch (Exception e) {
            e.printStackTrace();
        }
    }

    /**
     * GPIO write
     *
     * @param gpio  0~Number
     * @param value 0: Low 1: High
     */
    public void gpioWrite(int gpio, int value) {
        if (null != mGpioService) {
            try {
                mGpioService.gpioWrite(gpio, value);
            } catch (RemoteException e) {
                e.printStackTrace();
            }
        }
    }

    /**
     * GPIO read
     *
     * @param gpio 0~Number
     * @return 0: Low 1: High other：error
     */
    public int gpioRead(int gpio) {
        if (null != mGpioService) {
            try {
                return mGpioService.gpioRead(gpio);
            } catch (RemoteException e) {
                e.printStackTrace();
            }
        }
        return -1;
    }

    /**
     * GPIO direction
     *
     * @param gpio      0~Number
     * @param direction 0: input 1: output
     * @param value     0: Low 1: High
     */
    public void gpioDirection(int gpio, int direction, int value) {
        if (null != mGpioService) {
            try {
                mGpioService.gpioDirection(gpio, direction, value);
            } catch (RemoteException e) {
                e.printStackTrace();
            }
        }
    }

    /**
     * GPIO register key event
     *
     * @param gpio 0~Number
     */
    public void gpioRegKeyEvent(int gpio) {
        if (null != mGpioService) {
            try {
                mGpioService.gpioRegKeyEvent(gpio);
            } catch (RemoteException e) {
                e.printStackTrace();
            }
        }
    }

    /**
     * GPIO unregister key event
     *
     * @param gpio 0~Number
     */
    public void gpioUnregKeyEvent(int gpio) {
        if (null != mGpioService) {
            try {
                mGpioService.gpioUnregKeyEvent(gpio);
            } catch (RemoteException e) {
                e.printStackTrace();
            }
        }
    }

    /**
     * Get GPIO number
     *
     * @return <0: error other: GPIO number
     */
    public int gpioGetNumber() {
        if (null != mGpioService) {
            try {
                return mGpioService.gpioGetNumber();
            } catch (RemoteException e) {
                e.printStackTrace();
            }
        }
        return -1;
    }
}

```
