import { Injectable, NgZone, EventEmitter } from '@angular/core';
import { Subject, Observable } from 'rxjs';

@Injectable({
    providedIn: 'root'
})
export class WebsocketService {
    private messageSubject: Subject<ArrayBuffer> = new Subject();

    private websocket = null;

    private readonly connectionEvent = new EventEmitter<void>();

    private readonly closeEvent = new EventEmitter<void>();

    public constructor(private zone: NgZone) {}

    public connect(host: string): void {
        this.connectionEvent.emit();
        return;

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
                this.closeEvent.emit();
            });
        };
    }

    public disconnect() {
        if (this.websocket) {
            this.websocket.close();
            this.websocket = null;
        }

        // TODO: remove
        this.closeEvent.emit();
    }

    public getConnectionEventObservable(): Observable<void> {
        return this.connectionEvent.asObservable();
    }

    public getCloseEventObservable(): Observable<void> {
        return this.closeEvent.asObservable();
    }

    public getMessageObservable(): Observable<ArrayBuffer> {
        return this.messageSubject.asObservable();
    }

    public send(data: ArrayBuffer): void {
        /*if (!this.websocket) {
            throw new Error('WS is not open');
        }

        this.websocket.send(data);*/
        console.log('send', data);
    }
}
