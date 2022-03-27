#
#    SPDX-License-Identifier: GPL-3.0-or-later
#
#    This file is part of Piper0, a physically based renderer.
#    Copyright (C) 2022 Yingwei Zheng
#
#    This program is free software: you can redistribute it and/or modify
#    it under the terms of the GNU General Public License as published by
#    the Free Software Foundation, either version 3 of the License, or
#    (at your option) any later version.
#
#    This program is distributed in the hope that it will be useful,
#    but WITHOUT ANY WARRANTY; without even the implied warranty of
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#    GNU General Public License for more details.
#
#    You should have received a copy of the GNU General Public License
#    along with this program.  If not, see <https://www.gnu.org/licenses/>.

import os
import sys
from types import NoneType
import numpy as np
import json


def parse(lines: str):
    res = []
    for line in lines.splitlines():
        if len(line) == 0:
            continue
        k, v = list(map(lambda x: float(x), line.split(",")))
        res.append([k*1e3, v])

    while len(res) >= 2 and res[1][0] < 360:
        res.pop(0)

    while len(res) >= 2 and res[-2][0] > 830:
        res.pop(-1)

    if len(res) == 0:
        raise "Invalid data"

    return np.array(res)


if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("Please input the path to data files")
        exit(-1)

    path = sys.argv[1]

    materials = []

    for r, ds, fs in os.walk(path):
        for f in fs:
            if f.endswith(".csv"):
                name = f.removesuffix(".csv")
                materials.append((name, r+"/"+f))

    for name, eta in materials:
        print("Generating", name)

        data = ""
        with open(eta, "r", encoding="utf-8") as f:
            data = "".join(f.readlines())

        eta = None
        k = None
        if data.find("wl,k") != -1:
            eta = parse(data[data.find("wl,n")+4:data.find("wl,k")])
            k = parse(data[data.find("wl,k")+4:])
        else:
            eta = parse(data[data.find("wl,n")+4:])

        if type(k) != NoneType and np.all(np.transpose(k)[1] < 1e-3):
            k = None

        with open("ior/"+name+".json", "w", encoding="utf-8") as f:
            obj = {
                "Eta": {
                    "Type": "SampledSpectrumTexture",
                    "Array": eta.tolist()
                }
            }

            if type(k) != NoneType and len(k) > 0:
                obj["K"] = {
                    "Type": "SampledSpectrumTexture",
                    "Array": k.tolist()
                }

            json.dump(obj, f)
