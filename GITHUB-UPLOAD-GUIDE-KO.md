# GitHub 업로드 안내

## 저장소가 비어 있거나 다른 프로젝트에서 복제된 경우

`OStrands-0.8.0-Creature-PPlus-Hybrid-Final.zip` 전체 소스 패키지를 사용하세요.

1. ZIP을 PC의 새 폴더에 압축 해제합니다.
2. 압축 해제된 `OStrands-0.8.0-Creature-PPlus-Hybrid-Final` 폴더를 엽니다.
3. **폴더 자체가 아니라 내부 항목을** GitHub 저장소 최상위에 업로드합니다.
4. 저장소 최상위에서 `CMakeLists.txt`, `src/`, `data/`, `extern/`, `.github/workflows/windows-build.yml`이 바로 보여야 합니다.
5. 커밋 후 Actions 탭에서 `OStrands 0.8.0 Hybrid Windows build` 실행을 확인합니다.

ZIP 파일 자체를 GitHub에 올리는 것만으로는 소스가 압축 해제되거나 빌드되지 않습니다.

## 저장소가 `OStrands_0.8.0_Creature_PPlus_KO_Source.zip`과 정확히 같은 기반인 경우

변경된 파일만 담은 `OStrands-0.8.0-Creature-PPlus-Hybrid-Changed-Files.zip`을 사용할 수 있습니다. 이 ZIP은 저장소 상대경로(`src/`, `.github/`, `tools/` 등)를 유지합니다. 동일한 경로의 파일을 교체하고 새 파일을 추가한 뒤 커밋하세요.

저장소가 다른 0.8.0 소스나 0.7.8 소스에서 만들어졌다면 변경 파일만 적용하지 말고 전체 소스 패키지를 사용하세요.

## 빌드 결과

Actions가 성공하면 Artifacts에서 다음 항목을 내려받을 수 있습니다.

- `build/release/0_OStrands.dll`
- `OStrands-0.8.0-Creature-PPlus-Hybrid-DLL-ONLY-MO2.zip`

MO2 ZIP에는 DLL만 들어 있습니다. 원본 OStrands 0.8.0의 메시, 텍스처, INI를 유지하세요.
