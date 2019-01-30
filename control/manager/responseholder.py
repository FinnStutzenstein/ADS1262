class ResponseHolder:
    """
    Simple class to hold a value, in this case the response from the server
    """
    response = None

    def set(self, x):
        self.response = x

    def get(self):
        return self.response
