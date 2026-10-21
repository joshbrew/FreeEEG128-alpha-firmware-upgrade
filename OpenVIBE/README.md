# Historical OpenViBE integration

`freeeeg32/` contains the original FreeEEG32 acquisition-server driver and UI.
`openvibe-acquisition-server` is the original tracked executable. They are retained
for reference and are not part of the maintained firmware build or release ZIP.

The upgraded FreeEEG128 application sends 393-byte packets containing 128 signed
24-bit samples. The retained 32-channel driver expects a different packet size;
it has not been adapted or validated for this stream. Use the root launcher's
**Test recording** action to check delivery with the supplied 128-channel reader.

[Current packet format](../Firmware/Alpha/README.md#packet-format-and-throughput) /
[Recording checks and limits](../Flash/HELPERS.md).
