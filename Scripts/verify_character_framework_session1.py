"""Session 1 reflection/load probe for the installed character framework plugin."""

import unreal


EXPECTED_CLASSES = (
    "/Script/DiscGolfCharacterFramework.DiscGolfThrowComponent",
    "/Script/DiscGolfCharacterFramework.DiscGolfAnimInstance",
    "/Script/DiscGolfCharacterFramework.AnimNotify_DiscRelease",
    "/Script/DiscGolfCharacterFramework.DiscGolfCharacterProfile",
)

EXPECTED_STRUCTS = (
    "/Script/DiscGolfCharacterFramework.DGBodyProfile",
    "/Script/DiscGolfCharacterFramework.DGThrowStyle",
    "/Script/DiscGolfCharacterFramework.DGThrowIntent",
    "/Script/DiscGolfCharacterFramework.DGReleaseData",
)


def _require_object(path: str):
    reflected_object = unreal.load_object(None, path)
    if reflected_object is None:
        raise RuntimeError(f"Required reflected object did not load: {path}")
    unreal.log(f"SESSION1 REFLECTION OK: {path}")


def main():
    for class_path in EXPECTED_CLASSES:
        reflected_class = unreal.load_class(None, class_path)
        if reflected_class is None:
            raise RuntimeError(f"Required reflected class did not load: {class_path}")
        unreal.log(f"SESSION1 REFLECTION OK: {class_path}")

    for struct_path in EXPECTED_STRUCTS:
        _require_object(struct_path)

    unreal.log(
        "SESSION1 CHARACTER FRAMEWORK PASS: "
        f"{len(EXPECTED_CLASSES)} classes and {len(EXPECTED_STRUCTS)} structs loaded."
    )


if __name__ == "__main__":
    main()
