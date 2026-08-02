#!/usr/bin/env python3
"""Merge Android 7/8 CarrierConfig APK assets into Android 12 vendor.xml."""

import argparse
import copy
import re
import zipfile
from pathlib import Path
from xml.etree import ElementTree


ASSET_PATTERN = re.compile(r"assets/carrier_config_(\d{5,6})\.xml$")


def read_configs(apk_path: Path) -> dict[str, list[ElementTree.Element]]:
    configs: dict[str, list[ElementTree.Element]] = {}
    with zipfile.ZipFile(apk_path) as apk:
        for asset_name in apk.namelist():
            match = ASSET_PATTERN.fullmatch(asset_name)
            if not match:
                continue
            mcc_mnc = match.group(1)
            root = ElementTree.fromstring(apk.read(asset_name))
            entries = [node for node in root if node.tag == "carrier_config"]
            if not entries:
                raise ValueError(f"{apk_path}:{asset_name} has no carrier_config entry")
            configs[mcc_mnc] = entries
    return configs


def mcc_mnc_parts(mcc_mnc: str) -> tuple[str, str]:
    if len(mcc_mnc) not in (5, 6):
        raise ValueError(f"invalid MCC/MNC: {mcc_mnc}")
    return mcc_mnc[:3], mcc_mnc[3:]


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--cna", required=True, type=Path,
                        help="Chinese Android 8 CarrierConfig.apk")
    parser.add_argument("--doa", required=True, type=Path,
                        help="Japanese Android 7 CarrierConfig.apk")
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()

    # Apply the Japanese baseline first. The Chinese release then wins each
    # MCC/MNC collision, including 460xx China Mobile/Unicom/Telecom entries.
    configs = read_configs(args.doa)
    configs.update(read_configs(args.cna))

    root = ElementTree.Element("carrier_config_list")
    root.append(ElementTree.Comment(
        "Generated from 9008_DOA7 then 9008_CNA8; CNA wins duplicate MCC/MNCs."))
    for mcc_mnc in sorted(configs):
        mcc, mnc = mcc_mnc_parts(mcc_mnc)
        for entry in configs[mcc_mnc]:
            merged = copy.deepcopy(entry)
            merged.set("mcc", mcc)
            merged.set("mnc", mnc)
            root.append(merged)

    args.output.parent.mkdir(parents=True, exist_ok=True)
    ElementTree.indent(root, space="    ")
    ElementTree.ElementTree(root).write(
        args.output, encoding="utf-8", xml_declaration=True, short_empty_elements=True)
    print(f"wrote {sum(len(entries) for entries in configs.values())} entries for "
          f"{len(configs)} MCC/MNCs to {args.output}")


if __name__ == "__main__":
    main()
