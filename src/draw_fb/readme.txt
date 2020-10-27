compile:
gcc -o main main.c -lpng

// Run
sudo ./main dev-buffer-path image-path(*.png)
example:
sudo ./main /dev/fb0 1.png

