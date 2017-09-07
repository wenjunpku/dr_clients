How to build
---

1. Install cmake 

2. Download "dynamorio" on github

3. Use VS2013 cmd, run following command


```
mkdir ../build
cd ../build
<CMAKEPATH> -DDynamoRIO_DIR=<DYNAMORIO_HOME_PATH>/cmake ..
```

4. Use Visual Studio Open "ALL BUILD" project


===
RUN
===
```
./get_trace.sh examples/fibonacci/fibonacci_64.exe 00001
```
