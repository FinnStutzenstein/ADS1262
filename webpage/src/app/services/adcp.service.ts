import { Injectable } from '@angular/core';
import { PacketType, ADCPRepresentationService } from './adcp-representation.service';
import { take, timeout } from 'rxjs/operators';
import { StructService } from './struct.service';

const ADCPTimeout = 5000;

export type StatusCode = number;
const ADCPStatuscodes: { [code: number]: string } = {
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
    0x0f: 'RESPONSE_FFT_INVALID_WINDOW'
};

type ADCPPrefixCommand = [number, number];

const ADCP: {
    [prefix: string]: {
        [command: string]: ADCPPrefixCommand;
    };
} = {
    connection: {
        setType: [0x10, 0x00]
    }
};

@Injectable({
    providedIn: 'root'
})
export class ADCPService {
    public constructor(private ADCPRepService: ADCPRepresentationService, private structService: StructService) {
        this.ADCPRepService.getConnectionEventObservable().subscribe(() => {
            this.setConnectionType([PacketType.Debug, PacketType.Status]).then(
                response => {
                    console.log('RESP', ADCPStatuscodes[response]);
                },
                error => {
                    console.log(error);
                }
            );
        });
    }

    public async setConnectionType(types: PacketType[]): Promise<StatusCode> {
        console.log('set connection type', types);
        let allTypes = 0;
        types.forEach(type => {
            allTypes = allTypes | type;
        });
        this.sendPacket(ADCP.connection.setType, new Uint8Array([allTypes]).buffer as ArrayBuffer);
        return this.getStatusCodeResponse();
    }

    private async getStatusCodeResponse(): Promise<StatusCode> {
        const view = new DataView(await this.getResponse());
        if (view.byteLength < 1) {
            throw new Error('No bytes returned');
        }
        return view.getUint8(0);
    }

    private async getResponse(): Promise<ArrayBuffer> {
        return await this.ADCPRepService.getPacketObservable(PacketType.Response)
            .pipe(
                take(1),
                timeout(ADCPTimeout)
            )
            .toPromise();
    }

    private sendPacket(prefixCommand: ADCPPrefixCommand, data: ArrayBuffer): void {
        this.ADCPRepService.send(this.structService.toBuffer('BBA', prefixCommand[0], prefixCommand[1], data));
    }
}
