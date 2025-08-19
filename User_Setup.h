#define ILI9488_DRIVER

#define TFT_WIDTH  480
#define TFT_HEIGHT 320

#define TFT_MISO 12
#define TFT_MOSI 13
#define TFT_SCLK 14
#define TFT_CS   15
#define TFT_DC    2
#define TFT_RST  -1

#define TFT_BL    27
#define TFT_BACKLIGHT_ON HIGH

#define TFT_RGB_ORDER TFT_BGR
#define TFT_INVERSION_OFF

#define SPI_FREQUENCY       55000000
#define SPI_READ_FREQUENCY  20000000
#define SPI_TOUCH_FREQUENCY 2500000

// Optional: Touch support
#define TOUCH_CS   33
#define TOUCH_CLK  14
#define TOUCH_MOSI 13
#define TOUCH_MISO 12
#define TOUCH_IRQ  36

// Load fonts
#define LOAD_FONT2
#define LOAD_FONT4
#define LOAD_FONT6
#define LOAD_FONT7
#define LOAD_FONT8
#define LOAD_GFXFF
#define SMOOTH_FONT
