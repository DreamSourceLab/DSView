# libsigrokdecoder_spi-tpm

libsigrok stacked Protocol Decoder for TPM 2.0 & TPM 1.2 transactions from an SPI bus.
BitLocker Volume Master Key (VMK) is automatically extracted.

## How to use

### CLI

Example from a previous capture (Value Change Dump - .vcd format):

1. Identifying channels name:

`sigrok-cli -i capture.vcd --show`

2. Extracting BitLocker VMK (filter output to extract only VMK annotations)

`.\sigrok-cli.exe -i C:\Users\jovre\Documents\tpm_spi_comm_2.vcd -I vcd:numchannels=6 -P spi:wordsize=8:cs_polarity=active-high:miso=libsigrok4DSL.3:mosi=libsigrok4DSL.1:clk=libsigrok4DSL.2:cs=libsigrok4DSL.0,spi_tpm:tpm_version=1.2 -A spi_tpm=VMK`

***command output:***

`spi_tpm-1: VMK header: 2c0005000100000003200000`

`spi_tpm-1: VMK: 85e5xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxee00`

### GUI

Use and configure the SPI Protocol Decoder and then select SPI TPM Stack Decoder.
