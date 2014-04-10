#include "serializer/buf_ptr.hpp"

#include "math.hpp"

buf_ptr buf_ptr::alloc_zeroed(block_size_t size) {
    guarantee(size.ser_value() != 0);
    const size_t count = compute_in_memory_size(size);
    buf_ptr ret;
    ret.block_size_ = size;
    ret.ser_buffer_.init(malloc_aligned(count, DEVICE_BLOCK_SIZE));
    memset(ret.ser_buffer_.get(), 0, count);
    return ret;
}

scoped_malloc_t<ser_buffer_t> help_allocate_copy(const ser_buffer_t *copyee,
                                                 size_t amount_to_copy,
                                                 size_t reserved_size) {
    rassert(amount_to_copy <= reserved_size);
    void *buf = malloc_aligned(reserved_size, DEVICE_BLOCK_SIZE);
    memcpy(buf, copyee, amount_to_copy);
    memset(reinterpret_cast<char *>(buf) + amount_to_copy,
           0,
           reserved_size - amount_to_copy);
    return scoped_malloc_t<ser_buffer_t>(buf);
}

buf_ptr buf_ptr::alloc_copy(const buf_ptr &copyee) {
    guarantee(copyee.has());
    return buf_ptr(copyee.block_size(),
                   help_allocate_copy(copyee.ser_buffer_.get(),
                                      copyee.block_size().ser_value(),
                                      copyee.reserved().ser_value()));
}

void buf_ptr::resize_fill_zero(block_size_t new_size) {
    guarantee(new_size.ser_value() != 0);
    guarantee(ser_buffer_.has());

    uint32_t old_reserved = compute_in_memory_size(block_size_);
    uint32_t new_reserved = compute_in_memory_size(new_size);

    if (old_reserved == new_reserved) {
        if (new_size.ser_value() < block_size_.ser_value()) {
            // Set the newly unused part of the block to zero.
            // RSI: Performance? -- and can this entire function just go away?
            memset(reinterpret_cast<char *>(ser_buffer_.get()) + block_size_.ser_value(),
                   0,
                   block_size_.ser_value() - new_size.ser_value());
        }
    } else {
        // We actually need to reallocate.
        scoped_malloc_t<ser_buffer_t> buf
            = help_allocate_copy(ser_buffer_.get(),
                                 std::min(block_size_.ser_value(),
                                          new_size.ser_value()),
                                 new_reserved);

        ser_buffer_ = std::move(buf);
    }
    block_size_ = new_size;
}
