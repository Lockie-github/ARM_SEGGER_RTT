SECTIONS
{
  .rtt_cb (NOLOAD) :
  {
    . = ALIGN(4);
    KEEP(*(.rtt_cb))
    . = ALIGN(4);
  } >RAM

  .rtt_buffer (NOLOAD) :
  {
    . = ALIGN(4);
    KEEP(*(.rtt_buffer))
    . = ALIGN(4);
  } >RAM
}
