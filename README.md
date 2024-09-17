Click on a window to get its PID

Repurposed code from [xkill](https://gitlab.freedesktop.org/xorg/app/xkill) and [xprop](https://gitlab.freedesktop.org/xorg/app/xprop)

Use this instead, it's better:
```
xprop _NET_WM_PID
```

# Building
```console
git clone https://github.com/kivattt/xpidof
cd xpidof
sudo apt install libxmu-dev
./compile.sh
```
