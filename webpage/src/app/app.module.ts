import { BrowserModule } from '@angular/platform-browser';
import { NgModule } from '@angular/core';

import { AppRoutingModule } from './app-routing.module';
import { AppComponent } from './app.component';
import { WebsocketService } from './services/websocket.service';
import { ADCPRepresentationService } from './services/adcp-representation.service';

@NgModule({
    declarations: [AppComponent],
    imports: [BrowserModule, AppRoutingModule],
    providers: [WebsocketService, ADCPRepresentationService],
    bootstrap: [AppComponent]
})
export class AppModule {}
