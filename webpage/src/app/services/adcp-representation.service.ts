import { Injectable } from '@angular/core';
import { WebsocketService } from './websocket.service';
import { Subject, Observable } from 'rxjs';
import { StructService } from './struct.service';

export enum PacketType {
    Response = 0,
    Debug = 1,
    Status = 2,
    Data = 4,
    FFT = 8
}

@Injectable({
    providedIn: 'root'
})
export class ADCPRepresentationService {
    private packetSubjects: { [packetType: number]: Subject<ArrayBuffer> } = {};

    public constructor(private websocketService: WebsocketService, private structService: StructService) {
        this.websocketService.getMessageObservable().subscribe((data: ArrayBuffer) => {
            this.packetInput(data);
        });
    }

    // Make private.
    public packetInput(data: ArrayBuffer): void {
        if (data.byteLength < 3) {
            console.error('Did not get enough bytes', data);
            return;
        }

        let packetType: number;
        let length: number;
        [packetType, length] = this.structService.fromBuffer('BH', data) as [number, number];
        if (length !== data.byteLength - 3) {
            console.error('The amount of bytes in the message does not match with the given length of ADCP.');
        }

        const payload = data.slice(3, data.byteLength);
        if (this.packetSubjects[packetType]) {
            this.packetSubjects[packetType].next(payload);
        } else {
            console.log('Got unknown package with type ' + packetType.toString() + ' and length ' + length, payload);
        }
    }

    // For debugging
    public respOk(): void {
        const bytesString = '00010000';

        const view = new Uint8Array(bytesString.length / 2);
        for (let i = 0; i < bytesString.length; i += 2) {
            view[i / 2] = parseInt(bytesString.substring(i, i + 2), 16);
        }
        this.packetInput(view.buffer as ArrayBuffer);
    }
    public respError(): void {
        const bytesString = '00010009';

        const view = new Uint8Array(bytesString.length / 2);
        for (let i = 0; i < bytesString.length; i += 2) {
            view[i / 2] = parseInt(bytesString.substring(i, i + 2), 16);
        }
        this.packetInput(view.buffer as ArrayBuffer);
    }
    public respStatus(): void {
        const bytesString = '001F00001C04FF00e6b2000e0000000005a80000031c00410101130100000000f000';

        const view = new Uint8Array(bytesString.length / 2);
        for (let i = 0; i < bytesString.length; i += 2) {
            view[i / 2] = parseInt(bytesString.substring(i, i + 2), 16);
        }
        this.packetInput(view.buffer as ArrayBuffer);
    }

    public getPacketObservable(type: PacketType): Observable<ArrayBuffer> {
        if (!this.packetSubjects[type]) {
            this.packetSubjects[type] = new Subject<ArrayBuffer>();
        }
        return this.packetSubjects[type].asObservable();
    }

    public send(data: ArrayBuffer): void {
        this.websocketService.send(data);
    }
}
