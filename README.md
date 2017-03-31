# pia-resolv-fix
Very small C program for Ubuntu restores /etc/resolv.conf to a symbolic link when openvpn terminates unexpectedly

What does this do?
------------------

Private Internet Access client writes DNS server entries to resolve.conf, removing the symlink to /etc/resolvconf/resolv.conf 
that is kept dynamically up to date by the system. Sometimes, PIA doesn't close cleanly and fails to restore the symbolic 
link, causing the PIA DNS servers to be used even when the VPN is not in use, leading to poor DNS performance. This happens 
most often when the system is shutdown, and rarely during normal use.

This small utility installs a tiny C binary that is run by cron on each boot and every 15 minutes thereafter, restoring 
resolv.conf to a symbolic link to /etc/resolvconf/resolv.conf as it should be if it finds that openvpn (used by PIA) is not 
running. This makes sure that your local DNS servers get used when PIA is not running.

When should I use this?
-----------------------

```
dnsmasq[8542]: /etc/resolvconf/update.d/libc: Warning: /etc/resolv.conf is not a symbolic link to /run/resolvconf/resolv.conf
```

If you get errors like the above in your syslog, notice a drop in DNS performance, and use Private Internet Access on Ubuntu, 
this program might improve your DNS performance.

