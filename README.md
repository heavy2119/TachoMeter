# TachoMeter

Speed(Km) / RPM 

## Credits

- **Design:** Lee Chiwon 
- **Development:** Lee Chiwon 
- **RGB Matix : ** Lee Chiwon


<p align="left">
<img src="image/7.png" width="700" > 
</p>


## 🎥 Video

[![Video](image/8.png)](https://www.youtube.com/watch?v=Rn3uH0gUHZ0)

---

# Hardware Wire

<p align="left">
<img src="image/6.png" width="700">
</p>

| Function    | Arduino | Ring LED Matrix | Single LED  | Motor  |
| ----------- | ------- | ------- | --------- |
| 5V                 | 5V          | 5V       | 5V          |
| GND                | GND         | GND      | GND         |
| Ring LED Matrix    | D6          | IN       |             |
| Single LED Bar     | D5          |          | IN          |
| Motor              | D9,D10,D11  |          |             | 1(D9), 2,3(D10), 4(D11)

 
---

# Part List 

|   Name              | EA | Site | Note | 
| ------------------- | ------ | --- | ------ |
| Arduino Nano   | 1 |  [Buy](https://smartstore.naver.com/misoparts/products/5779944205)| 
| 8X8 LED Matrix    | 1 |  [Buy](https://smartstore.naver.com/misoparts/products/13199208576)| 
| 8 LED Bar    | 1 |  [Buy](https://smartstore.naver.com/misoparts/products/9434328063)| 
| 4T Smoke Acrylic    | 1 |  [Buy](https://smartstore.naver.com/me41)| 3D Print 폴더내 도면가지고 의뢰 하세요.


# 3D Print

| Name |  EA  | Note |
| --- |  ------ |------ |
| <p align="left"><img src="Image/15.png" width="300"></p>  |  1EA  | 
| <p align="left"><img src="Image/16.png" width="300"></p>  |  1EA  | 
| <p align="left"><img src="Image/17.png" width="300"></p>  |  1EA  | 

# Simhub Arduino Setting
<p align="left">
<img src="Image/18.png" width="700">
</p>

 * 8 LED Bar *
1. WS2812B RGB leds Count  = 8
2. Data(DIN digital pin number = 6
3. encoding = RGB encoding


 * RGB Matrix *
1. Enable WS2812B 8x8 matrix = ON
2. Data(DIN digital pin number = 3


# Multiple Arduino Setting
1. Compile Arduino 1 (Change the Device Name)</p>
2. Compile Arduino 2 (Change the Device Name)</p>
3. Arduino 1: Set Start Position = 1 and LED Count = 8</p>
4. Arduino 2: Set Start Position = 9 and LED Count = 8</p>

<p align="left">
<img src="Image/23.png" width="700">
</p>
