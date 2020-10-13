from utils import Generator, camelcase


def arguments(args):
    result = ""

    index = 0
    for arg in args:
        result += f"{args[arg]} {arg}"
        if index + 1 < len(args):
            result += ", "

        index += 1

    return result


def make_message(request_name: str, prot, args):
    result = '{\n' + prot['properties']['magic'] + \
        ',\n' + 'ServerMessageType::' + request_name.upper() + ',\n'

    index = 0
    for arg in args:
        result += f"{arg}"
        if index + 1 < len(args):
            result += ",\n"

        index += 1

    return result + '}'


def request_handler(gen: Generator, request_name: str, prot, request):
    gen.emit(
        f"{camelcase(request_name)+'Response'} {request_name}({arguments(request['arguments'])})")

    gen.emit("{")
    gen.push_ident()
    gen.emit()

    gen.emit(
        f"ServerMessage message = {make_message(request_name, prot, request['arguments'])};")

    gen.emit("connection_send(message)")

    gen.pop_ident()
    gen.emit("}")
    gen.emit("")


def signal_handler(gen: Generator, signal_name: str,  prot, signal):
    gen.emit(
        f"void {signal_name}({arguments(signal['arguments'])})")
    gen.emit("{")

    gen.emit("}")
    gen.emit("")


def connection(gen: Generator, prot, peer):
    gen.emit("#pragma once")

    gen.emit("")
    gen.emit("// Don't edit this code !")
    gen.emit("// It was generated by ipc-compiler.py")

    gen.emit("")
    gen.emit(
        f"#include <protocols/{camelcase(prot['properties']['name'])}Protocol.h>")
    gen.emit("#include <libsystem/io/Connection.h>")
    gen.emit("")

    gen.emit("namespace protocols")
    gen.emit("{")
    gen.emit("")

    gen.emit(f"class {camelcase(peer['name'])}Connection")
    gen.emit("{")

    gen.emit("private:")
    gen.push_ident()
    gen.emit("Connection* _connection;")
    gen.emit("")
    gen.pop_ident()

    gen.emit("private:")
    gen.push_ident()

    if len(peer["requests"]) > 0:
        gen.emit(
            "/* --- Requests ------------------------------------------------------------- */")
        gen.emit("")

        for req in peer["requests"]:
            request_handler(gen, req, prot, peer["requests"][req])

    if len(peer["signals"]) > 0:
        gen.emit(
            "/* --- Signales ------------------------------------------------------------- */")
        gen.emit("")

        gen.emit("")
        for sig in peer["signals"]:
            signal_handler(gen, sig, prot, peer["signals"][sig])

    gen.pop_ident()
    gen.emit("};")

    gen.emit("")
    gen.emit("}")