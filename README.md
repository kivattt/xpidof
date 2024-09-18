Click on a window to get its PID

Repurposed code from [xkill](https://gitlab.freedesktop.org/xorg/app/xkill) and [xprop](https://gitlab.freedesktop.org/xorg/app/xprop)

Use this instead, it's better:
```
xprop _NET_WM_PID
```

Here's an alias you can put in your `.bashrc` file to get the same output as this program:
```bash
alias xpidof="xprop _NET_WM_PID | awk '\$NF != \"found.\"{print \$NF}'"
```

# Building
```console
git clone https://github.com/kivattt/xpidof
cd xpidof
sudo apt install libxmu-dev
./compile.sh
```
