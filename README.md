# tabover

A very simple terminal based X Window switcher.

![tabover in xterm](https://raw.githubusercontent.com/imwally/tabover/master/tabover.png)

## Why?

Spreading application windows across multiple virtual desktops is an
easy way to lose track of what's open and where they are. Typically I
dedicate a single application to a desktop but I don't always remember
which one. In most modern desktop environments the Alt-Tab switcher
has an option to switch between windows on all desktops but not in
most tiling window managers. tabover fills that missing role for
tiling WMs.

## Usage

* `Tab` or `j` select next
* `~` or `k` select previous
* `Return` activate window
* `q` quit

## Requirements

* [xcb](http://xcb.freedesktop.org/)
* WM that supports [ewmh](https://en.m.wikipedia.org/wiki/EWMH)


