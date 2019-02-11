import { Injectable } from '@angular/core';
import * as bigInt from 'big-integer';

const sizeMap: { [formatsymbol: string]: number } = {
    b: 1,
    B: 1,
    h: 2,
    H: 2,
    i: 4,
    I: 4,
    q: 8,
    Q: 8,
    f: 4
};

/**
 * Provides functionality to put data into an ArrayBuffer and get data from
 * a buffer. Always in little endian.
 *
 * `toBuffer` puts all given data together to an ArrayBuffer. The types for the format are:
 * b (int8), B (uint8), h (int16), H (uint16), i (int32), I (uint32), q (int64), Q (uint64),
 * A (ArrayBuffer), f (32 bit float).
 * All corresponsing TypeScript types are number, except for the ArrayBuffer and q/Q. These are BitIntegers.
 *
 * `fromBuffer` Does this in reverse, except that an `A` is just allowed at the end to catch all remaining bytes.
 *
 * The static `printBuffer` method is nice for debugging purposes.
 */
@Injectable({
    providedIn: 'root'
})
export class StructService {
    public constructor() {}

    /**
     * Prints an arraybuffer.
     *
     * @param buffer The buffer
     */
    public static printBuffer(buffer: ArrayBuffer): void {
        const array = new Uint8Array(buffer);
        const blocksize = 8;
        let pos = 0;
        let str = '';
        while (pos < buffer.byteLength) {
            str += ('0' + array[pos].toString(16)).slice(-2) + ' ';
            pos++;
            if (pos % blocksize === 0) {
                console.log(str);
                str = '';
            }
        }
        if (pos % blocksize !== 0) {
            console.log(str);
        }
    }

    public toBuffer(format: string, ...args: (number | bigInt.BigInteger | ArrayBuffer)[]): ArrayBuffer {
        if (format.length !== args.length) {
            throw new Error("Format and number of arguments doesn't match");
        }

        // Check arg types.
        for (let i = 0; i < format.length; i++) {
            const symbol = format.charAt(i);
            if (symbol === 'A') {
                if (args[i].constructor.name !== 'ArrayBuffer') {
                    throw new Error(`Argument ${i + 1} is not an ArrayBuffer`);
                }
            } else if (symbol === 'q' || symbol === 'Q') {
                if (args[i].constructor.name !== 'Integer') {
                    throw new Error(`Argument ${i + 1} is not a BigInteger`);
                }
            } else if (args[i].constructor.name !== 'Number') {
                throw new Error(`Argument ${i + 1} is not a number`);
            }
        }

        // Get needed size and validate format string
        let size = 0;
        for (let i = 0; i < format.length; i++) {
            const symbol = format.charAt(i);
            if (sizeMap[symbol]) {
                size += sizeMap[symbol];
            } else if (symbol === 'A') {
                size += (<ArrayBuffer>args[i]).byteLength;
            } else {
                throw new Error(`Format symbol ${symbol} is not known`);
            }
        }

        // Construct the buffer.
        const buffer = new ArrayBuffer(size);
        const view = new DataView(buffer);
        let pos = 0;
        for (let i = 0; i < format.length; i++) {
            const symbol = format.charAt(i);

            let arrayView;
            switch (symbol) {
                case 'b':
                    view.setInt8(pos, <number>args[i]);
                    break;
                case 'B':
                    view.setUint8(pos, <number>args[i]);
                    break;
                case 'h':
                    view.setInt16(pos, <number>args[i], true);
                    break;
                case 'H':
                    view.setUint16(pos, <number>args[i], true);
                    break;
                case 'i':
                    view.setInt32(pos, <number>args[i], true);
                    break;
                case 'I':
                    view.setUint32(pos, <number>args[i], true);
                    break;
                case 'q':
                case 'Q':
                    const num: bigInt.BigInteger = <bigInt.BigInteger>args[i];
                    const hex: string = this.get8ByteHexFromBigInt(num);
                    // reverse for little endian.
                    const array = new Uint8Array(
                        hex
                            .match(/[\da-f]{2}/gi)
                            .map(val => parseInt(val, 16))
                            .reverse()
                    );

                    if (num.isNegative()) {
                        // two's complements....
                        // invert the array
                        let j: number;
                        for (j = 0; j < 8; j++) {
                            array[j] = ~array[j];
                        }
                        j = 0;
                        let overflow = true;
                        while (overflow && j < 8) {
                            // get number and add +1. if then >255,we had an overflow.
                            const number = array[j] + 1;
                            array[j] = number;
                            overflow = number > 255;
                            j++;
                        }
                    }

                    arrayView = new Uint8Array(buffer, pos, 8);
                    arrayView.set(array, 0);
                    break;
                case 'f':
                    view.setFloat32(pos, <number>args[i], true);
                    break;
                case 'A':
                    const bufferToAdd = <ArrayBuffer>args[i];
                    arrayView = new Uint8Array(buffer, pos, bufferToAdd.byteLength);
                    arrayView.set(new Uint8Array(bufferToAdd), 0);
                    break;
            }

            if (sizeMap[symbol]) {
                pos += sizeMap[symbol];
            } else {
                // symbol === 'A'
                size += (<ArrayBuffer>args[i]).byteLength;
            }
        }

        return buffer;
    }

    public fromBuffer(format: string, buffer: ArrayBuffer): (number | bigInt.BigInteger | ArrayBuffer)[] {
        const result: (number | bigInt.BigInteger | ArrayBuffer)[] = [];

        // Check for A.
        for (let i = 0; i < format.length - 1; i++) {
            if (format.charAt(i) === 'A') {
                throw new Error('A is just allowed at the end of the format string.');
            }
        }

        // Get needed size and validate format string
        let size = 0;
        for (let i = 0; i < format.length; i++) {
            const symbol = format.charAt(i);
            if (sizeMap[symbol]) {
                size += sizeMap[symbol];
            } else if (symbol !== 'A') {
                throw new Error(`Format symbol ${symbol} is not known`);
            }
        }

        if (buffer.byteLength < size) {
            throw new Error('Buffer is too small');
        }

        const view = new DataView(buffer);
        let pos = 0;
        for (let i = 0; i < format.length; i++) {
            const symbol = format.charAt(i);

            switch (symbol) {
                case 'b':
                    result.push(view.getInt8(pos));
                    break;
                case 'B':
                    result.push(view.getUint8(pos));
                    break;
                case 'h':
                    result.push(view.getInt16(pos, true));
                    break;
                case 'H':
                    result.push(view.getUint16(pos, true));
                    break;
                case 'i':
                    result.push(view.getInt32(pos, true));
                    break;
                case 'I':
                    result.push(view.getUint32(pos, true));
                    break;
                case 'q':
                case 'Q':
                    // get 8 byte hex
                    const array = new Uint8Array(buffer, pos, 8);
                    let negative = false;
                    if (symbol === 'q' && array[7] & 0x80) {
                        // We do have a negative number, make it positive and
                        // after converting the positive number to bigint, inverse
                        // the sign there.
                        negative = true;
                        this.twosComplementInPlace(array);
                    }
                    // This step makes a hex string in big endian.
                    let str = '';
                    for (let j = 0; j < 8; j++) {
                        str += ('0' + array[7 - j].toString(16)).slice(-2);
                    }
                    const num = bigInt(str, 16);
                    if (negative) {
                        result.push(num.multiply(bigInt['-1']));
                    } else {
                        result.push(num);
                    }
                    break;
                case 'f':
                    result.push(view.getFloat32(pos, true));
                    break;
                case 'A':
                    // Get buffer from current pos to the end.
                    const bufferToAdd = new ArrayBuffer(buffer.byteLength - pos);
                    if (bufferToAdd.byteLength > 0) {
                        const arrayView = new Uint8Array(buffer, pos, bufferToAdd.byteLength);
                        const bufferToAddView = new Uint8Array(bufferToAdd);
                        bufferToAddView.set(new Uint8Array(arrayView), 0);
                    }
                    result.push(bufferToAdd);
                    break;
            }

            if (sizeMap[symbol]) {
                pos += sizeMap[symbol];
            } else {
                // symbol === 'A'
                break; // no more..
            }
        }

        return result;
    }

    // Byte order must be little endian
    private twosComplementInPlace(array: Uint8Array): void {
        // invert the array
        let i;
        for (i = 0; i < array.length; i++) {
            array[i] = ~array[i];
        }
        // Do +1 with overflow to next byte.
        i = 0;
        let overflow = true;
        while (overflow && i < array.length) {
            // get number and add +1. if then >255,we had an overflow.
            const number = array[i] + 1;
            array[i] = number;
            overflow = number > 255;
            i++;
        }
    }

    private get8ByteHexFromBigInt(num: bigInt.BigInteger): string {
        let hex = num.toString(16);
        if (hex.startsWith('-')) {
            hex = hex.slice(1);
        }
        return ('0000000000000000' + hex).slice(-16);
    }
}
