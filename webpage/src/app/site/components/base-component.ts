export class BaseComponent {
    public raiseError: (msg: string) => void = msg => alert(msg);

    public raiseErrorWithText: (text: string) => (string) => void = text => (msg: string) => alert(text + ': ' + msg);

    public constructor() {}
}
