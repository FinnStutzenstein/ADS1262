import { Injectable } from '@angular/core';
import { PacketType, ADCPRepresentationService } from './adcp-representation.service';
import { StructService } from './struct.service';
import { Subject, Observable } from 'rxjs';

@Injectable({
    providedIn: 'root'
})
export class DebugService {
    private debugMessageSubject: Subject<string> = new Subject<string>();

    private decoder = new TextDecoder('ascii');

    public constructor(private ADCPRepService: ADCPRepresentationService, private structService: StructService) {
        this.ADCPRepService.getPacketObservable(PacketType.Debug).subscribe((msg: ArrayBuffer) => {
            const text = this.decoder.decode(msg);
            text.split('\n')
                .filter(x => !!x)
                .forEach(part => {
                    this.debugMessageSubject.next(part);
                });
        });
    }

    public getDebugMessageObservable(): Observable<string> {
        return this.debugMessageSubject.asObservable();
    }
}
