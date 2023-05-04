Echo client. Run along with an echo server.

If run the code with VSCode Microsoft C/C++ plugin, it opens a powershell terminal with UTF-8 encoding (`chcp` outputs 65001) and when echoing Chinese character, the echo gets garbled. Need to run `chcp 936` in advance to set encoding to GBK and run with command line or compile with option `-fexec-charset=GBK`. I guess the typing tool gives process GBK encoded bytes since windows is GBK by default, so it's needed to set terminal encoding to be GBK to display Chinese character.

By the way, VSCode header -> Terminal -> New Terminal, run `chcp` and you can see terminal opened in this way is encoded with GBK, and git bash did't happen garbling without special operation.