# gwnb_lid
Keyboard and mouse driver for phytium platform notebooks

## How to build
```bash
make
sudo make install
```

## dkms build
```bash
sudo cp gwnb_lid /usr/src/gwnb_lid-0.1/
sudo dkms add -m gwnb_lid
sudo dkms build -m gwnb_lid -v 0.1
sudo dkms install -m gwnb_lid -v 0.1
```