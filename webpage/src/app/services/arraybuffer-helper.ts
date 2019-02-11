/**
 * Appends the both given buffers
 */
export function appendBuffers(b1: ArrayBuffer, b2: ArrayBuffer): ArrayBuffer {
    const tmp = new Uint8Array(b1.byteLength + b2.byteLength);
    tmp.set(new Uint8Array(b1), 0);
    tmp.set(new Uint8Array(b2), b1.byteLength);
    return tmp.buffer as ArrayBuffer;
}
