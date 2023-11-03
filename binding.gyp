{
    "targets": [
        {
            "target_name": "casparengine",
            "sources":["./src/heat.cpp", "./src/ADC.cpp", "./src/control.cpp", "./src/data.cpp",
            "./src/casparapi.cpp", "./src/GaussNewton3.cpp","./src/GaussNewton4.cpp", "./src/qiagen.cpp", "./src/rt_nonfinite.cpp",
            "./src/setup.cpp", "./src/configINI.cpp"],
            "libraries": [ "-l", "wiringPi"],
            "include_dirs" : ["<!(node -e \"require('nan')\")"]
        },
    ]
}