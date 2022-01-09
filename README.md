# 1. 概要

esp32 を活用して I2S オーディオ入力(ADC)の視覚化としてよくある LED 風バンド表示と、おまけレベルな周波数スペクトル表示を行うプログラムです。

手っ取り早く動作結果を知りたい方のために[動画](/img/VIDEO000.mp4)にしました。音声は完全カットしています。
![TOP](/img/TOP.jpg)

# 2. 経緯

2021年10月頃なにげに esp32 の[資料](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32/api-reference/peripherals/i2s.html)を読んでいて I2S のピン定義に
mck_io_num MCK in out pin. Note that ESP32 supports setting MCK on GPIO0/GPIO1/GPIO3 only と記述されているのを見てついに
MCK(マスタークロック) 出せるようになったんだと思いました。以前、某氏からせがまれて Raspberry Pi 3 Model B 向けな TEXAS INSTRUMENTS (以下tiと略) の
[PCM1808](https://www.ti.com/lit/ds/symlink/pcm1808.pdf)を実装したボード製作した事も思い出しました(笑) その際に製作した予備ボードを引っ張り出して改造
(外部電源入力ピン出し。I2S関連端子ピン出し。外部クロック入力可化。MD0/MD1/FMTのジャンパ化)したものを esp32 で活用して見ようと仕事の合間を見ながら
ちまちま進めて結果がここです。

そもそも某氏から ti の PCM1808 旧Burr Brown製 実装ボード製作をせがまれたのもトラ技2017年1月号で [Pumpkin Pi](http://einstlab.web.fc2.com/RaspberryPi/PumpkinPi.html) の記事を読んだらしく手持ちのアナログ音源(SP/EP/LP/カセットテープ/MD/などｗ)を 24bit/96KHz で取り込みしたいというオカルトチックな欲望と安価に製作依頼をして楽したいと言う狙いがあったらしい(笑)
[マルツ](https://www.marutsu.co.jp/select/list/detail.php?id=258)で完成品も部品セットもあるから自分で注文して作れと言っても<br>
「arecord して flac 化するスクリプトは自分で書くからさ〜 老眼すぎてもう手ハンダ無理(TдT) PCM1808 だけでいいんよ。頼むよ〜(^o^)」と某氏は言ってた気がする(笑)

![トラ技2017年1月号表紙](/img/トラ技2017年1月.jpg)

# 3. 開発環境

以下で行いました。利用させて頂いたツール/ライブラリの作者orグループor会社の方々には感謝申し上げます。

ハードウェア<br>
esp32-wrover-b 搭載開発ボード(esp32-wroom-32 搭載開発ボードも可。以下esp32と略)<br>
ILI9341 SPI 接続 240x320 RGB565 タッチパネル付き(以下TFTと略)<br>
PCM1808 チップ実装ボード(自作。以下PCM1808ボードと略)<br>

PCM1808ボードは[秋月電子通商](https://akizukidenshi.com/catalog/default.aspx)な部品で元々 PCM1808 マスターで 48K or 96K ジャンパピンで組んでたのが丸わかりです(笑)
![PCM1808ボード](/img/PCM1808ボード.jpg)

ソフトウェア<br>
[arduino IDE](https://www.arduino.cc/en/software) Linux 64bits ver 1.8.19<br>
[Arduino core for the ESP32](https://github.com/espressif/arduino-esp32) ver 2.0.2<br>
[TFT_eSPI](https://github.com/Bodmer/TFT_eSPI) ver 2.4.2<br>
[arduinoFFT](https://github.com/kosme/arduinoFFT) ver 1.5.6<br>

# 4. ハード編

TFT と esp32 のつなぎ (CS 分離で VSPI 使用)

    2.8 TFT          esp32
      1 VCC       --- 3V3
      2 GND       --- GND
      3 CS        --- 15 GPIO15
      4 RESET     ---  4 GPIO4
      5 DC        ---  2 GPIO2
      6 SDI(MOSI) --- 23 VSPI MOSI      
      7 SCK       --- 18 VSPI CLK
      8 LED       --- 32 GPIO32
      9 SDO(MISO) --- 19 VSPI MISO
     10 T_CLK     --- 18 VSPI CLK
     11 T_CS      --- 21 GPIO21
     12 T_DIN     --- 23 VSPI MOSI
     13 T_DO      --- 19 VSPI MISO
     14 T_IRQ     --- NC
    
      1 SD_CS     --- NC
      2 SD_MOSI   --- NC
      3 SD_MISO   --- NC
      4 SD_SCK    --- NC

PCM1808ボードは ti [資料](https://www.ti.com/lit/ds/symlink/pcm1808.pdf)より 8.2 Typical Application の赤枠内を実装したものです。<br>
(5)の破線囲みは実装していません。

![PCM1808 Typical Application](/img/PCM1808_002.jpg)

PCM1808ボードと esp32 のつなぎ

    PCM1808ボード    esp32
    5V   ----------- 5V
    3.3V ----------- 3V3
    GND  ----------- GND
    SCKI -----------  0 GPIO0
    LRCK ----------- 27 GPIO27
    BCK  ----------- 26 GPIO26
    DOUT ----------- 25 GPIO25

つなぎ全景を示します。
![つなぎ全景](/img/全景.jpg)

esp32 の GPIO0 をそのまま PCM1808ボードの SCKI へ。なるべく距離は短くしていますが、こんなのじゃ工業製品として失格です(爆笑)<br>
むき出しボードで esp32 WiFi 切らずジャンプワイヤでループ作っているのでノイズがのる事は想定内です。<br>
[PCM1808](https://www.ti.com/lit/ds/symlink/pcm1808.pdf) を 5V 端子が無い esp32 な開発ボードで利用したい場合は 5V が利用できる外部電源を活用して
GND は共用したら良いと思います。

# 5. ソフト&使用編

TFT_eSPI の設定(Arduino/libraries/TFT_eSPI/User_Setup.h)は以下のとおりです。

    #define ILI9341_DRIVER
    #define TFT_BL              32
    #define TFT_BACKLIGHT_ON    HIGH
    #define TFT_MISO            19
    #define TFT_MOSI            23
    #define TFT_SCLK            18
    #define TFT_CS              15
    #define TFT_DC              2
    #define TFT_RST             4
    #define TOUCH_CS            21
    #define LOAD_GLCD
    #define LOAD_FONT2
    #define LOAD_FONT4
    #define LOAD_FONT6
    #define LOAD_FONT7
    #define LOAD_FONT8
    #define LOAD_GFXFF
    #define SMOOTH_FONT
    #define SPI_FREQUENCY       40000000
    #define SPI_READ_FREQUENCY  20000000
    #define SPI_TOUCH_FREQUENCY 2500000

TFT_eSPI のタッチパネル機能を利用する場合には TFT の個体差があるためキャリブレーション(スケッチ例→TFT_eSPI→Generic→Touch_calibrate)プログラムを一度書き込んで得られた配列値と差し替えてください。

    i2sDinDisp (i2sDinDisp.ino)

    15 uint16_t calData[5] = { 462, 3364, 336, 3324, 7 };


スプライトは利用していますが実装としてフレームバッファ 1 枚として利用してるにすぎません。目盛り文字列/凡例/グラフ目盛りなど毎回描画してると言う事です(笑)

esp32 で psram が利用できる場合は FFT 用のメモリは psram 側ヒープより取得します。スプライトの色深度も 16 として RGB565 で利用し LED 風レベル表示を TFT
向かって上から下へ赤→黄→緑とし、それぞれ範囲を 20 % , 30 % , 残りとしてグラデーションした色値を利用するようにしました。

タッチパネル接続が有効で TFT タッチするとメニュー表示します。項目タッチでそれぞれの状態表示に切り替わります。
![MENU](/img/MENU.jpg)

i2s_config_t の dma_buf_count と dma_buf_len が最新の[資料](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32/api-reference/peripherals/i2s.html)で解説されていました。
dma_buf_count は dma_desc_num のエイリアス。dma_desc_num の解説を読んだ範囲で理解したのは dma バッファ転送に関連したディスクリプタ(管理識別子やらポインタ
などを一意に示す事と思います)を保持する個数で、ここで指定した回数ぶん i2s_read() を繰り返して読み込む必要があると理解しました。dma_buf_len は dma_frame_num
のエイリアス。dma_frame_num の解説を読んだ範囲で理解したのは1回に読み込むフレーム数。興味ある方は一度目をとおされたら良いと思います。

FFT 後の周波数解析結果をバンド化するアルゴリズムは昔組んでた方式で恐縮ですが各帯域(#define BANDS 8)で帯域加算(fft count/2 ^ n/BANDSより前後の帯域も含ませる)
した結果をデシベル値変換(10*log10(Mag^2))します。しかし、そのままその値を画面表示に利用すると、とても大きな値になるため特定の値(#define DB_RANGE 45)で割って
整数 0〜n(n = #define DATA_RANGE 100) 化する方式です。
#define BANDS 8 の値を変化させるとその数と定義されたサンプリングレートと FFT 数より適切に各帯域を算出する方式にしました。

#define DB_RANGE 45 の値は人によるかもしれないのでおまけレベルな周波数スペクトル表示にて観察して全体を見通して各帯域の加算結果を 35 ぐらいで割った方が
良いと感じる方も居ると思いますし 42 ぐらいで割った方が適切と感じる方も居ると思います。普段聞いている楽曲の種類に依存するのかも(笑) 大きくし過ぎると LED 風
レベル表示の上部がいつも遊んでいる状態になります。小さくし過ぎると LED 風レベル表示が常に最大を示す状態になります。

おまけレベルな周波数スペクトル表示では 0dB をサイン波形の符号付き 32bit データとして最大振幅した値より #define ZERO_DB 42.797582 // 42.797582 = 10 * log10( 138 * 138 )
としました。1.0 〜  0 〜 -1.0 な正規化データで大きさor強さ 138 は大きすぎかもしれません(笑)

おまけレベルな周波数スペクトル表示の縦軸の dB レンジ値(#define DB_RANGE_VALUE 99)は [PCM1808](https://www.ti.com/lit/ds/symlink/pcm1808.pdf) SNR or ダイナミック
レンジ 99dB と記載されたスペックより -99dB としています。DB_RANGE_VALUE を縦軸の最低値として目盛りをふっています。

    i2sDinDisp (i2sDinDisp.ino)

    12 int idBList[] = { 6, 10, 15, 20, 30, 40, 50, 60, 70, 80, 90, DB_RANGE_VALUE };

![PCM1808 Features](/img/PCM1808_001.jpg)

アナログ音源が体感無音のときのおまけレベルな周波数スペクトル表示を示します。

![体感無音](/img/体感無音.jpg)

高級アンプは手持ちしていません。SONY/AVセレクター/[SB-RX200S](https://www.sony.jp/ServiceArea/impdf/pdf/2177407051.pdf)のアナログ出力を PCM1808ボードに入れています。
軽く 15 年は経過してるアナログ専用AVセレクターです(爆笑) それでも十分許容範囲な体感無音状態だと思います。

異なる I2S ADC チップで SNR or ダイナミックレンジは常に同じとは仮定できないので異なる I2S ADC チップを使用する場合はチップの資料を見て最低値を決めるか、とりあえず
99→120 にして 1KHz -10dB な波形データを ADC して dB レンジ値を探すのも手です。

1KHz -10dB な波形データの状態表示
![1KHz -10dB](/img/1KHz-10dB.jpg)

おまけレベルな周波数スペクトル表示は FFT 数がとても少ないので周波数値ズレ(32KHz で FFT 512 数だと周波数分解能は 62.5Hz ですｗ)は発生すると思ってください。
仮に周波数分解能を上げるために #define WAVE_SIZE (BLOCK_SIZE * DMA_DESC_NUM) の #define DMA_DESC_NUM 16 を 64 とか 128 にしてもベタ処理のため表示が目に見て重くなります。
最低でも FFT 8192 数(フレーム 8192 数)取れればより良いと思います。その為にはソフト構造的に I2S Read を Core #0 TASK で行ってリングバッファに入れ Core #1 TASK
とミューテックスかけてリングバッファより取り出して FFT し Core #1 TASK で表示すると言う形式だと行けそうです。が真面目な製品化みたいなのでしませんでした(笑)

周波数分解能が悪いための低音域の状態表示

125Hz -20dB な波形データの状態表示
![125Hz -20dB](/img/125Hz-20dB.jpg)

250Hz -20dB な波形データの状態表示
![250Hz -20dB](/img/250Hz-20dB.jpg)

500Hz -20dB な波形データの状態表示
![500Hz -20dB](/img/500Hz-20dB.jpg)

周波数分解能が悪くても中高音域の状態表示

2KHz -20dB な波形データの状態表示
![2KHz -20dB](/img/2KHz-20dB.jpg)

4KHz -20dB な波形データの状態表示
![4KHz -20dB](/img/4KHz-20dB.jpg)

8KHz -20dB な波形データの状態表示
![8KHz -20dB](/img/8KHz-20dB.jpg)

# 6. I2S 観察編

esp32 を I2S マスターとして PCM1808ボードとの I2S ロジアナ観察結果です。

サンプリング周波数 96KHz は、頑張って周波数算出してこの BCK になったと言う事だと思います。正確な 96KHz 実装に向けては i2s_config.use_apll i2s_config.fixed_mclk I2S_???_REG を
弄る必要があるのかなと思います。
![96KHz](/img/ESP32_MASTER_I2S_96K.png)

サンプリング周波数 64KHz は OK です。
![64KHz](/img/ESP32_MASTER_I2S_64K.png)

サンプリング周波数 48KHz も OK です。
![48KHz](/img/ESP32_MASTER_I2S_48K.png)

サンプリング周波数 32KHz も OK です。
![32KHz](/img/ESP32_MASTER_I2S_32K.png)

サンプリング周波数 16KHz も OK です。
![16KHz](/img/ESP32_MASTER_I2S_16K.png)

サンプリング周波数 8KHz も OK です。
![8KHz](/img/ESP32_MASTER_I2S_8K.png)

esp32 を I2S スレーブとしてサンプリング周波数 96KHz とすると以下の結果が出てきました。
![ESP32_I2S_SLAVE_96KHz](/img/ESP32_SLAVE_I2S_96K_001.png)

「出来ないお(ﾉД`)ｼｸｼｸ」と出てくるのは良いのですがこの時の i2s_driver_install() と i2s_set_pin() の戻り値は ESP_OK でした。ちょっと寂しかったです。

# 7. その他

日本的固有ネタ(トラ技/秋月電子通商)に溢れてるので英訳は不要と思います(笑)

以下、決まり文句です。

「このプログラムを再利用した事により重大な事故、人的怪我、社内査定等の損害には一切責任を持てません。再利用はあくまで自己責任でお願いいたします」

以上です。


