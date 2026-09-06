# romulon SWD tail

PIN   | name
------|--------
SWDIO |
GND   |
SWCLK |

To handle the Serial Wire Debug (SWD) protocol on the RP2040, the PIO state machine must manage a bi-directional data line (SWDIO) while maintaining a continuous, synchronized clock signal (SWCLK).The trickiest part of SWD in PIO is the turnaround cycle (Trn). When switching from writing (host driving the line) to reading (target driving the line), the line must be tri-stated for exactly one clock cycle to prevent electrical contention.Here is a complete, optimized PIO program (swd.pio) designed to handle both sending commands and reading data payloads.

```
// Example: Sending an 8-bit SWD Request Frame
void swd_write_frame(PIO pio, uint sm, uint offset, uint32_t data, uint8_t num_bits) {
    // Force the SM to jump to the write subroutine
    pio_sm_exec(pio, sm, pio_encode_jmp(offset + swd_offset_write_bits));
    
    // Push the bit count (minus 1) then the actual bits
    pio_sm_put_blocking(pio, sm, num_bits - 1);
    pio_sm_put_blocking(pio, sm, data);
}

// Example: Reading a response (like a 3-bit ACK or 32-bit RAM data)
uint32_t swd_read_frame(PIO pio, uint sm, uint offset, uint8_t num_bits) {
    // Force the SM to jump to the read subroutine
    pio_sm_exec(pio, sm, pio_encode_jmp(offset + swd_offset_read_bits));
    
    // Push the number of bits we want to read (minus 1)
    pio_sm_put_blocking(pio, sm, num_bits - 1);
    
    // Wait for the PIO to complete the turnaround, read bits, and push to RX FIFO
    return pio_sm_get_blocking(pio, sm);
}
```

a PIO swd bit banger suggested by gbot

```
.program swd
.side_set 1             ; The sideset pin is always SWCLK

; ---------------------------------------------------------------------
; SUBROUTINE: WRITE BITS (Host to Target)
; Pushes 'X' number of bits from the OSR onto the SWDIO pin.
; TX FIFO must contain: [Number of bits minus 1] then [The data payload]
; ---------------------------------------------------------------------
public write_bits:
    pull block          side 0  ; Pull bit-count from FIFO, clear clock
    out x, 32           side 0  ; Move bit-count into X register
    pull block          side 0  ; Pull the actual data payload into OSR
    
bit_loop_tx:
    out pins, 1         side 0  ; Drive 1 bit onto SWDIO, clock low
    jmp x-- bit_loop_tx side 1  ; Drive clock high, loop until X = 0
    jmp wrap_target     side 0  ; Clean up clock, return to idle/ready

; ---------------------------------------------------------------------
; SUBROUTINE: READ BITS WITH TURNAROUND (Target to Host)
; Switches SWDIO to input, waits 1 turn cycle, reads 'Y' bits into ISR.
; TX FIFO must contain: [Number of bits minus 1 to read]
; ---------------------------------------------------------------------
public read_bits:
    pull block          side 0  ; Pull read bit-count from FIFO
    out y, 32           side 0  ; Move bit-count into Y register
    
    ; --- Turnaround Cycle ---
    set pindirs, 0      side 0  ; Switch SWDIO pin to INPUT
    nop                 side 1  ; Wait 1 clock cycle for target to take over
    
bit_loop_rx:
    in pins, 1          side 0  ; Sample 1 bit from SWDIO into ISR, clock low
    jmp y-- bit_loop_rx side 1  ; Drive clock high, loop until Y = 0
    
    push block          side 0  ; Push completed data to RX FIFO for CPU
    set pindirs, 1      side 0  ; Turn SWDIO back to OUTPUT (Safe state)
    ; --- Post-Turnaround ---
    nop                 side 1  ; 1 clock cycle for host to retake line
    
% c-sdk {
static inline void swd_program_init(PIO pio, uint sm, uint offset, uint pin_swclk, uint pin_swdio) {
    pio_sm_config c = swd_program_get_default_config(offset);

    // Map the SWDIO pin to the IN, OUT, and SET (for pindirs) paths
    sm_config_set_out_pins(&c, pin_swdio, 1);
    sm_config_set_in_pins(&c, pin_swdio);
    sm_config_set_set_pins(&c, pin_swdio, 1);

    // Map the SWCLK pin to the side-set path
    sm_config_set_sideset_pins(&c, pin_swclk);

    // Set pin directions in the GPIO matrix
    pio_gpio_init(pio, pin_swclk);
    pio_gpio_init(pio, pin_swdio);
    
    // Start with clock as output (low), data as output (high/idle)
    pio_sm_set_consecutive_pindirs(pio, sm, pin_swclk, 1, true);
    pio_sm_set_consecutive_pindirs(pio, sm, pin_swdio, 1, true);

    // Configure shift registers (SWD is LSB first)
    sm_config_set_out_shift(&c, true, false, 32); // Shift right, no autopull
    sm_config_set_in_shift(&c, true, false, 32);  // Shift right, no autopush

    // Set clock divider (e.g., divide system clock down to 1-10 MHz)
    sm_config_set_clkdiv(&c, 25.0f); 

    // Load configuration and start state machine
    pio_sm_init(pio, sm, offset, &c);
    pio_sm_set_enabled(pio, sm, true);
}
}
```

To handle the Serial Wire Debug (SWD) protocol on the RP2040, the PIO state machine must manage a bi-directional data line (SWDIO) while maintaining a continuous, synchronized clock signal (SWCLK).
The trickiest part of SWD in PIO is the turnaround cycle (Trn). When switching from writing (host driving the line) to reading (target driving the line), the line must be tri-stated for exactly one clock cycle to prevent electrical contention.
Here is a complete, optimized PIO program (swd.pio) designed to handle both sending commands and reading data payloads.
------------------------------
## 1. The PIO Assembly Code (swd.pio)
Save this file as swd.pio. It uses Sideset to drive the clock (SWCLK) automatically on every instruction, ensuring perfectly timed transitions.

.program swd
.side_set 1             ; The sideset pin is always SWCLK

; ---------------------------------------------------------------------
; SUBROUTINE: WRITE BITS (Host to Target)
; Pushes 'X' number of bits from the OSR onto the SWDIO pin.
; TX FIFO must contain: [Number of bits minus 1] then [The data payload]
; ---------------------------------------------------------------------
public write_bits:
    pull block          side 0  ; Pull bit-count from FIFO, clear clock
    out x, 32           side 0  ; Move bit-count into X register
    pull block          side 0  ; Pull the actual data payload into OSR
    
bit_loop_tx:
    out pins, 1         side 0  ; Drive 1 bit onto SWDIO, clock low
    jmp x-- bit_loop_tx side 1  ; Drive clock high, loop until X = 0
    jmp wrap_target     side 0  ; Clean up clock, return to idle/ready

; ---------------------------------------------------------------------
; SUBROUTINE: READ BITS WITH TURNAROUND (Target to Host)
; Switches SWDIO to input, waits 1 turn cycle, reads 'Y' bits into ISR.
; TX FIFO must contain: [Number of bits minus 1 to read]
; ---------------------------------------------------------------------
public read_bits:
    pull block          side 0  ; Pull read bit-count from FIFO
    out y, 32           side 0  ; Move bit-count into Y register
    
    ; --- Turnaround Cycle ---
    set pindirs, 0      side 0  ; Switch SWDIO pin to INPUT
    nop                 side 1  ; Wait 1 clock cycle for target to take over
    
bit_loop_rx:
    in pins, 1          side 0  ; Sample 1 bit from SWDIO into ISR, clock low
    jmp y-- bit_loop_rx side 1  ; Drive clock high, loop until Y = 0
    
    push block          side 0  ; Push completed data to RX FIFO for CPU
    set pindirs, 1      side 0  ; Turn SWDIO back to OUTPUT (Safe state)
    ; --- Post-Turnaround ---
    nop                 side 1  ; 1 clock cycle for host to retake line
    
% c-sdk {
static inline void swd_program_init(PIO pio, uint sm, uint offset, uint pin_swclk, uint pin_swdio) {
    pio_sm_config c = swd_program_get_default_config(offset);

    // Map the SWDIO pin to the IN, OUT, and SET (for pindirs) paths
    sm_config_set_out_pins(&c, pin_swdio, 1);
    sm_config_set_in_pins(&c, pin_swdio);
    sm_config_set_set_pins(&c, pin_swdio, 1);

    // Map the SWCLK pin to the side-set path
    sm_config_set_sideset_pins(&c, pin_swclk);

    // Set pin directions in the GPIO matrix
    pio_gpio_init(pio, pin_swclk);
    pio_gpio_init(pio, pin_swdio);
    
    // Start with clock as output (low), data as output (high/idle)
    pio_sm_set_consecutive_pindirs(pio, sm, pin_swclk, 1, true);
    pio_sm_set_consecutive_pindirs(pio, sm, pin_swdio, 1, true);

    // Configure shift registers (SWD is LSB first)
    sm_config_set_out_shift(&c, true, false, 32); // Shift right, no autopull
    sm_config_set_in_shift(&c, true, false, 32);  // Shift right, no autopush

    // Set clock divider (e.g., divide system clock down to 1-10 MHz)
    sm_config_set_clkdiv(&c, 25.0f); 

    // Load configuration and start state machine
    pio_sm_init(pio, sm, offset, &c);
    pio_sm_set_enabled(pio, sm, true);
}
}

------------------------------
## 2. How the C/C++ Driver Uses It
To communicate with the target, your core design CPU will send instructions sequentially to the PIO's TX FIFO using the entry points (offset + swd_offset_write_bits and offset + swd_offset_read_bits).
Here is the flow pattern your C code will execute to send an SWD transaction:

// Example: Sending an 8-bit SWD Request Framevoid swd_write_frame(PIO pio, uint sm, uint offset, uint32_t data, uint8_t num_bits) {
    // Force the SM to jump to the write subroutine
    pio_sm_exec(pio, sm, pio_encode_jmp(offset + swd_offset_write_bits));
    
    // Push the bit count (minus 1) then the actual bits
    pio_sm_put_blocking(pio, sm, num_bits - 1);
    pio_sm_put_blocking(pio, sm, data);
}
// Example: Reading a response (like a 3-bit ACK or 32-bit RAM data)uint32_t swd_read_frame(PIO pio, uint sm, uint offset, uint8_t num_bits) {
    // Force the SM to jump to the read subroutine
    pio_sm_exec(pio, sm, pio_encode_jmp(offset + swd_offset_read_bits));
    
    // Push the number of bits we want to read (minus 1)
    pio_sm_put_blocking(pio, sm, num_bits - 1);
    
    // Wait for the PIO to complete the turnaround, read bits, and push to RX FIFO
    return pio_sm_get_blocking(pio, sm);
}

------------------------------
## 3. Core SWD Sequence to Polling RTT
Once this PIO framework is running, your higher-level loop will execute the three core steps of an SWD memory read to grab the RTT buffers:

   1. Send Request: Use swd_write_frame() to send the SWD read command (includes target memory location address).
   2. Read ACK: Use swd_read_frame(..., 3) to read the 3-bit acknowledgment from the target. If it returns 0b001 (OK), proceed.
   3. Read Data: Use swd_read_frame(..., 32) to pull the 32-bit word directly out of the target's RAM.

This gives you a robust, hardware-level interface that can run at several megahertz without stalling your main system software loop.
Would you like me to map out the exact 8-bit byte formats for the standard SWD Read/Write command packets, or do you have the ARM CoreSight specification handy for your design?

