import { Injectable, NgZone, EventEmitter } from '@angular/core';
import { Subject, Observable } from 'rxjs';

@Injectable({
    providedIn: 'root'
})
export class WebsocketService {
    private messageSubject: Subject<ArrayBuffer> = new Subject();

    private websocket = null;

    private connectionEvent = new EventEmitter<void>();

    public constructor(private zone: NgZone) {}

    public connect(host: string): void {
        this.websocket = new WebSocket(`ws://${host}/ws`);
        this.websocket.binaryType = 'arraybuffer';

        // connection established. If this connect attept was a retry,
        // The error notice will be removed and the reconnectSubject is published.
        this.websocket.onopen = (event: Event) => {
            console.log('open', event);
            this.zone.run(() => {
                this.connectionEvent.emit();
            });
        };

        this.websocket.onmessage = (event: MessageEvent) => {
            this.zone.run(() => {
                this.messageSubject.next(event.data);
            });
        };

        this.websocket.onclose = (event: CloseEvent) => {
            console.log('close', event);
            this.zone.run(() => {
                this.websocket = null;
                console.error('Websocket closed', event);
            });
        };
    }

    public getConnectionEventObservable(): Observable<void> {
        return this.connectionEvent.asObservable();
    }

    public getMessageObservable(): Observable<ArrayBuffer> {
        return this.messageSubject.asObservable();
    }

    public send(data: ArrayBuffer): void {
        if (!this.websocket) {
            throw new Error('WS is not open');
        }

        this.websocket.send(data);
    }
}
