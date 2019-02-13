import { Injectable, NgZone, EventEmitter } from '@angular/core';

import { Subject, Observable } from 'rxjs';

/**
 * This service establishes and manages the websocket connection.
 */
@Injectable({
    providedIn: 'root'
})
export class WebsocketService {
    /**
     * Subject for incomming messages
     */
    private messageSubject: Subject<ArrayBuffer> = new Subject();

    /**
     * The websocket object.
     */
    private websocket = null;

    /**
     * Event fires, when the connection is established.
     */
    private readonly connectionEvent = new EventEmitter<void>();

    /**
     * Event fires, when the connection is closed.
     */
    private readonly closeEvent = new EventEmitter<void>();

    public constructor(private zone: NgZone) {}

    /**
     * Connects to the given host.
     *
     * @param host The host to connect to.
     */
    public connect(host: string): void {
        this.websocket = new WebSocket(`ws://${host}/ws`);
        this.websocket.binaryType = 'arraybuffer';

        // connection established. If this connect attept was a retry,
        // The error notice will be removed and the reconnectSubject is published.
        this.websocket.onopen = (event: Event) => {
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
            this.zone.run(() => {
                this.websocket = null;
                this.closeEvent.emit();
            });
        };
    }

    /**
     * Disconnects the active connection.
     */
    public disconnect() {
        if (this.websocket) {
            this.websocket.close();
            this.websocket = null;
        }
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
        if (!this.websocket) {
            throw new Error('WS is not open');
        }

        this.websocket.send(data);
    }
}
