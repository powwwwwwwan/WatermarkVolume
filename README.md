> \\\*\\\*Let the computer express itself. Let it roar.\\\*\\\*



这是一个纯 C++ 编写的单文件小程序。它剥夺了你手动调节音量的权利，将系统音量的控制权交给了内存水位。

This is a pure C++ single-file mini-program. It strips away your right to manually adjust the volume and hands the control over to your RAM usage.



Because manual adjustment is boring？



Map system memory usage to audio volume and visualize it as a water level.

![演示效果](effect.gif)

Just a fun project exploring system data → visual/audio feedback.\*   \*\*️ Dual Mapping Modes\*\*

&#x20;   \*   \*\*Full Mapping (1:1)\*\*: 简单粗暴。内存吃满，音量拉满。

&#x20;       Simple and brutal. Max memory means max volume.

&#x20;   \*   \*\*Custom Mapping (线性映射)\*\*: 根据你常用的内存数值区间，进行更细腻的线性转化。

&#x20;       A delicate linear transformation based on your commonly used memory range.



\---



\### ️ How to use

1\. 编译并运行这个 `.cpp` 文件。

&#x20;  Compile and run this `.cpp` file.

2\. 打开几个大型软件，或者跑个死循环。

&#x20;  Open some heavy software, or run an infinite loop, and listen to it.



\*Built with C++ \& Windows API. No fancy engines, just raw logic.\*

