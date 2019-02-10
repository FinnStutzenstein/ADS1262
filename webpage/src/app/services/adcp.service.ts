import { Injectable } from '@angular/core';
import { PacketType, ADCPRepresentationService } from './adcp-representation.service';
import { take, timeout } from 'rxjs/operators';
import { StructService } from './struct.service';

const ADCPTimeout = 5000;

export type StatusCode = number;
export const ADCPStatuscodes: { [code: number]: string } = {
    0x00: 'RESPONSE_OK',
    0x01: 'RESPONSE_MESSAGE_TOO_SHORT',
    0x02: 'RESPONSE_INVALID_PREFIX',
    0x03: 'RESPONSE_INVALID_COMMAND',
    0x04: 'RESPONSE_TOO_FEW_ARGUMENTS',
    0x05: 'RESPONSE_NO_MEMORY',
    0x06: 'RESPONSE_NOT_ENABLED',
    0x07: 'RESPONSE_NO_MEASUREMENTS',
    0x08: 'RESPONSE_TOO_MUCH_MEASUREMENTS',
    0x09: 'RESPONSE_MEASUREMENT_ACTIVE',
    0x0a: 'RESPONSE_NO_SUCH_MEASUREMENT',
    0x0b: 'RESPONSE_NO_ENABLED_MEASUREMENT',
    0x0d: 'RESPONSE_FFT_NO_MEMORY',
    0x0e: 'RESPONSE_FFT_INVALID_LENGTH',
    0x0f: 'RESPONSE_FFT_INVALID_WINDOW',
    0x10: 'RESPONSE_ADC_RESET',
    0x11: 'RESPONSE_CALIBRATION_TIMEOUT',
    0x12: 'RESPONSE_SOMETHING_IS_NOT_GOOD',
    0x13: 'RESPONSE_WRONG_REFERENCE_PINS',
    0x14: 'RESPONSE_MESSAGE_TOO_LONG',
    0x15: 'RESPONSE_MESSAGE_TYPE_NOT_SUPPORTED'
};

export type ADCPPrefixCommand = [number, number];

export const ADCP: {
    [prefix: string]: {
        [command: string]: ADCPPrefixCommand;
    };
} = {
    connection: {
        setType: [0x10, 0x00]
    },
    measurement: {
        start: [0x12, 0x01],
        stop: [0x12, 0x02]
    },
    adc: {
        updateState: [0x13, 0x07]
    }
};

@Injectable({
    providedIn: 'root'
})
export class ADCPService {
    public constructor(private ADCPRepService: ADCPRepresentationService, private structService: StructService) {}

    public async getStatusCodeResponse(): Promise<void> {
        const resp = await this.getResponse();
        const code = this.processStatuscode(resp);

        if (code !== 0) {
            throw new Error(ADCPStatuscodes[code]);
        }
    }

    public async getResponse(): Promise<ArrayBuffer> {
        return await this.ADCPRepService.getPacketObservable(PacketType.Response)
            .pipe(
                take(1),
                timeout(ADCPTimeout)
            )
            .toPromise();
    }

    public processStatuscode(buffer: ArrayBuffer): number {
        if (buffer.byteLength < 1) {
            throw new Error('No bytes returned');
        }

        return this.structService.fromBuffer('B', buffer)[0] as number;
    }

    public sendPacket(prefixCommand: ADCPPrefixCommand, data?: ArrayBuffer): void {
        if (!data) {
            data = new ArrayBuffer(0);
        }
        this.ADCPRepService.send(this.structService.toBuffer('BBA', prefixCommand[0], prefixCommand[1], data));
    }
}
