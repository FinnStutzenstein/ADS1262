import { Injectable } from '@angular/core';
import { PacketType, ADCPRepresentationService } from './adcp-representation.service';
import { StructService } from './struct.service';
import { Subject, Observable } from 'rxjs';

@Injectable({
    providedIn: 'root'
})
export class DebugService {
    private readonly debugMessageLineSubject: Subject<string> = new Subject<string>();

    private decoder = new TextDecoder('ascii');

    public constructor(private ADCPRepService: ADCPRepresentationService, private structService: StructService) {
        this.ADCPRepService.getPacketObservable(PacketType.Debug).subscribe((msg: ArrayBuffer) => {
            const text = this.decoder.decode(msg);
            text.split('\n')
                .filter(x => !!x)
                .forEach(part => {
                    this.debugMessageLineSubject.next(part);
                });
        });
    }

    public getDebugMessageLineObservable(): Observable<string> {
        return this.debugMessageLineSubject.asObservable();
    }
}
