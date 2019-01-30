def parse_number(something):
    """
    Tkes something that should be parsed to a number. Currently checks for prefixes
    like 0x (hex), 0b (bin) and 0 (oct). Also normal numbers (as string or integer) are parsed.
    Raises a value error, if something goes wrong.
    """
    if isinstance(something, str):
        if something.startswith('0x'):
            return int(something[2:], 16)
        elif something.startswith('0b'):
            return int(something[2:], 2)
        elif something.startswith('0') and len(something) > 1:
            return int(something[1:], 8)
        else:
            return int(something)
    else:
        return int(something)
