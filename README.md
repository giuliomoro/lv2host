# An lv2 host and a Bela example program

Pre-requisites:
- for the host `liblilv-dev`. 
- for the example, add: `calf-plugins`
- for convenience, add: `lilv-utils`

### Bela setup

With internet access on the board do:
```
apt-get install liblilv-dev calf-plugins lilv-utils
```

Alternatively, you can try to install from your computer:
download https://raw.githubusercontent.com/giuliomoro/bela-random/master/cross-apt-get-install.sh and run
```
./cross-apt-get-install.sh liblilv-dev calf-plugins lilv-utils
```

Build and run the Bela program. From the IDE it shoudl just work. At the command line, youshould specify the extra library:
```
make -C ~/Bela PROJECT=lv2host LDFLAGS=-llilv-0 run
```



