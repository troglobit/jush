jush - just a useless shell
===========================
[![Travis Status][]][Travis]

jush is an almost useless shell for UNIX systems.  It is developed on
Linux but is regularly tested on FreeBSD as well.

![The jush shell](jush.png)
> _"I'm writing my own shell, it's nothing fancy like bash or anything ..."_

The project use the de facto standard GNU configure and build system to
ensure portability, so all you need is to run the `./configure` script
to get up and running.  If you check out the sources from GIT, first run
`./autogen.sh` to create the necessary files -- this also requires the
GNU autoconf and automake tools.

Goals:
- Have fun
- Be an example of [editline][] use

Targets:
- [X] Pipes
- [X] Backgrounding and job control
- [X] Redirect
- [X] Environment variables
- [ ] Command completion
- [X] Filename completion


[Travis]:        https://travis-ci.com/troglobit/jush
[Travis Status]: https://travis-ci.com/troglobit/jush.png?branch=master
[editline]:      https://github.com/troglobit/editline
