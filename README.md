# Implementation-of-SOCKS-4-protocol
實作SOCKS 4 protocol，SOCKS 4類似防火牆中的proxy，決策哪些連線是被允許的。
- 支援single-connection的連線，如一般的網頁。
- 支援multi-connection的連線，如FTP protocol。
- 支援FTP protocol傳輸超過1GB的檔案
- 使用Boost.Asio實作，請確認您的電腦是否有此函式庫。

## Usage
產生執行檔：```make```  
執行server：```./socks_server [port]```  

## Documents
- SOCKS 4: https://www.openssh.com/txt/socks4.protocol
- SOCKS 4A: https://www.openssh.com/txt/socks4a.protocol

## SOCKS 4功能簡介
- Connect：一般single-connection的網路連線。
- Bind：處理multi-connection protocol的網路連線，如FTP。

## socks.conf
- socks server將依據socks.conf內容決定允許連線至哪些ip address的服務。
- 可以設定多條規則，如可以有多條permit c的規則
### 格式
允許所有連線：
```
permit c *.*.*.*
permit b *.*.*.*
```
允許來自 140.113.xx.xx (來自交大ip)的connection request及任何來源的：  
```
permit c 140.113.*.*
permit b *.*.*.*

```

## Demo
使用Firefox demo。
### Firefox proxy setting
請先設定proxy為socks server架設機器的ip及對應的port
![image](https://user-images.githubusercontent.com/96563567/225553898-ec327ec5-f148-4ce8-9140-89de4511a45a.png)

### Demo scenario
#### Connect
1. 允許所有連線
2. 僅允許連線至140.113.xx.xx (交大ip)
3. 僅允續連線至123.45.67.89
4. 允許所有連線  

https://user-images.githubusercontent.com/96563567/225580293-8e54cd4a-e2b6-427d-b790-221d24d17f76.mp4



#### Bind
1. 允許所有連線（傳輸4.6GB的檔案）
3. 僅允續連線至123.45.67.89
4. 允許所有連線
