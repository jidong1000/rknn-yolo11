采用交叉编译，在../cpp的目录下输入
bash build-Linux.sh -t rk3588
-t后面是你的设备，根据自己的设备修改

video\_demo是获取摄像头进行目标检测的demo，\_zero是零拷贝技术
零拷贝即采用DMA减小内存和CPU的压力

该源码采用野火的开源代码，但是野火的代码在rga处理时，有时候有零输入报错，解决方式是直接绕过rga，在rga之前用opencv的resize函数强制转化成640\*640，解决报错

自训练的模型放在../model里

所有的依赖环境在requirements.txt里，可根据这个pip install 对应的版本

