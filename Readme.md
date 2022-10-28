# Broadcast Tree Protocol - NS-3

This repository contains the BTP implementation for NS-3


## Installation
- Download [ns-3](https://www.nsnam.org/releases/)
- [Install ns-3](https://www.nsnam.org/wiki/Installation)
- Create folder 'broadcast' in scratch folder
- Clone repository to the newly 'broadcast' folder
- Move 'EEBTPTag.h' and 'EEBTPTag.cc' to 'src/wifi/model'
- Edit src/wifi/wscript
  - Add "'model/EEBTPTag.cc'" to section 'obj.source'
  - Add "'model/EEBTPTag.h'" to section 'headers.source'


## Usage
Start a simple simulation by running './waf --run="broadcast --nWifi=10 --linearEnergyModel=false --eebtp=true --width=100 --height=100"'

- 'nWifi' specifies the number of Wi-Fi stations to simulate
- 'linearEnergyModel' specifies if the 'CustomTxEnergyModel' is used (false) or not (true)
- With 'eebtp=true' the EEBT-Protocol is used
- 'width' and 'height' is used to set the area where the nodes are deployed
