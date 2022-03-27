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
import numpy as np
import json

if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("Please input the root path to mitsuba2's source")
        exit(-1)

    path = sys.argv[1]+"/resources/data/ior"

    materials = []

    for r, ds, fs in os.walk(path):
        for f in fs:
            if f.endswith(".eta.spd"):
                name = f.removesuffix(".eta.spd")
                materials.append(
                    (name, r+"/"+f, r+"/"+f.replace(".eta.spd", ".k.spd")))

    for name, eta, k in materials:
        print("Generating", name)

        eta = np.loadtxt(eta)
        k = np.loadtxt(k)

        with open("ior/"+name+".json", "w", encoding="utf-8") as f:
            obj = {
                "Eta": {
                    "Type": "SampledSpectrumTexture",
                    "Array": eta.tolist()
                },
                "K": {
                    "Type": "SampledSpectrumTexture",
                    "Array": k.tolist()
                }
            }

            json.dump(obj, f)
