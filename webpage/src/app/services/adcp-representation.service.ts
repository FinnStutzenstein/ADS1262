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

    public constructor(private websocketService: WebsocketService, structService: StructService) {
        this.websocketService.getMessageObservable().subscribe((data: ArrayBuffer) => {
            if (data.byteLength < 3) {
                console.error('Did not get enough bytes', data);
                return;
            }

            let packetType: number;
            let length: number;
            [packetType, length] = structService.fromBuffer('BH', data) as [number, number];
            if (length !== data.byteLength - 3) {
                console.error('The amount of bytes in the message does not match with the given length of ADCP.');
            }

            const payload = data.slice(3, data.byteLength);
            if (this.packetSubjects[packetType]) {
                this.packetSubjects[packetType].next(payload);
            } else {
                console.log(
                    'Got unknown package with type ' + packetType.toString() + ' and length ' + length,
                    payload
                );
            }
        });
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

    public getConnectionEventObservable(): Observable<void> {
        return this.websocketService.getConnectionEventObservable();
    }
}
