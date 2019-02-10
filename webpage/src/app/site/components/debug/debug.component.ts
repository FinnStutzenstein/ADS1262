import { Component } from '@angular/core';

import { DebugService } from 'src/app/services/debug.service';

@Component({
    selector: 'app-debug',
    templateUrl: './debug.component.html',
    styleUrls: ['./debug.component.scss']
})
export class DebugComponent {
    public lines: string[] = [];

    public constructor(private debugService: DebugService) {
        this.debugService.getDebugMessageLineObservable().subscribe(msg => {
            this.lines.push(msg);
        });
    }
}
