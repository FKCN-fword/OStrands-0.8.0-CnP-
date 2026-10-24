# OStrands 0.8.0 Hybrid — 최종 통합 소스

## 목표 구성

이 프로젝트는 다음 네 부분을 결합한 개인 테스트용 브랜치입니다.

1. **공식 OStrands 0.8.0**: 신규 렌더링·물리·위치 보정 코드를 기반으로 사용합니다.
2. **크리처 지원**: 크리처 생식기 노드 대체 탐색과 인간형 전용 필터 제거, 크리처 분사 위치 처리를 유지합니다.
3. **SexLab P+ 2.19.0**: `sslThreadController.GetOrgasmCount(actor)`가 없으면 `sslActorAlias.GetOrgasmCount()`를 호출합니다. 독립형 `SLSO.esp`는 요구하지 않습니다.
4. **안정성 및 비용 절감**: 오래된 Papyrus 응답 폐기, 호출 실패 후 재시도 지연, 새 유체 영역이 필요할 때만 호스트 목록을 생성합니다.

한국어 설정 메뉴와 0.8.0의 `[Alignment]` 페이지를 유지합니다.

## 카운터·중복 정책

- 카운터 조회 간격은 P+ 별칭 폴링에서 약 **0.35초**입니다.
- 첫 유효 응답은 기준값으로만 저장합니다. P+가 최근 상태를 복원할 수 있으므로 이전 장면의 횟수를 새 효과로 재생하지 않습니다.
- 카운터가 증가하면 증가분을 계산합니다.
- 한 번의 조회에서 여러 번 증가한 경우, 증가분은 로그에 기록하지만 **효과 요청은 최대 한 번으로 병합**합니다. 10초 정책을 우회해 효과가 한꺼번에 생성되지 않도록 하기 위함입니다.
- 카운터가 감소하면 초기화/복원으로 간주하고 새 값을 기준값으로 재동기화합니다. 해당 샘플에서는 효과를 생성하지 않습니다.
- 한 번의 응답에서 카운터가 16보다 많이 증가하면 비정상 점프/오래된 상태일 수 있으므로 새 값을 기준값으로 저장하고 효과 폭주를 만들지 않습니다.
- 동일 액터의 효과 요청에 대한 기존 **10초 재요청 제한(`kRepeat`)**을 유지합니다.
- `FluidManager`에 이미 같은 액터의 emitter가 있으면 기존 코드가 추가 emitter 요청을 거부합니다. 이 통합본은 대기열을 추가하지 않습니다.
- Papyrus 반환값이 정수가 아니거나 호출에 실패하면 최소 5초 동안 재시도합니다. 타임아웃 난 이전 alias 라운드의 늦은 응답은 폐기합니다.

이 정책은 카운터 증가분을 감지하되, 연속 효과를 대기열에 쌓지 않습니다. 짧은 간격의 연속 절정마다 반드시 하나씩 효과를 생성해야 한다는 정책과는 다릅니다. 실제 사용자 패턴 및 10초 제한을 고려해 버스트 생성을 억제하는 보수적인 정책입니다.

## 설치 산출물

게임에는 원본 OStrands 0.8.0의 메시, 텍스처, `OStrands.ini`를 유지하고 **`SKSE/Plugins/0_OStrands.dll`만 교체**하도록 DLL 전용 MO2 ZIP을 생성합니다.

- `OStrands-0.8.0-Creature-PPlus-Hybrid-DLL-ONLY-MO2.zip`
- P+의 `sslActorAlias.pex`나 `sslActorLibrary.pex`를 덮어쓰지 않습니다.
- ESP/ESL 및 Papyrus 스크립트는 추가하지 않습니다.
- 다른 OStrands 버전의 리소스를 섞지 않습니다.

## 빌드

GitHub Actions 워크플로는 Windows 2022, MSVC, CommonLibSSE-NG v12.1.0, vcpkg manifest를 사용합니다. `sccache`와 vcpkg 바이너리 캐시를 설정해 재빌드 비용을 낮춥니다.

로컬 Windows 빌드가 필요한 경우 Visual Studio 2022 C++ 도구, CMake 3.25+, Ninja, vcpkg가 필요합니다. `extern/CommonLibSSE-NG`가 없으면 프로젝트 루트에서 실행합니다:

```powershell
git clone --depth 1 --branch v12.1.0 https://github.com/alandtse/CommonLibSSE-NG extern/CommonLibSSE-NG
```

그다음 `Build-0.8-Creature-PPlus-KO.cmd`를 실행합니다.

## 검사 및 검증 범위

`python tools/audit_hybrid_source.py`는 파일 및 소스 구조의 정적 불변 조건을 검사합니다. 정적 검사는 C++ 컴파일이나 게임 동작을 대신하지 않습니다.

현재 패키지를 이 응답 환경에서 Windows/MSVC로 컴파일하거나 Skyrim에 실행하지 않았습니다. GitHub Actions 빌드 성공 후 반드시 별도 MO2 프로필에서 다음을 확인하세요.

1. 게임 시작 및 새 장면 시작
2. P+ `GetOrgasmCount()`의 성공 응답 로그
3. 인간형 장면의 효과 생성
4. 크리처 장면의 생식기 노드 탐색과 효과 생성
5. 카운터 증가·초기화·장면 종료 뒤 오래된 응답 처리
6. 동일 액터 10초 중복 제한과 emitter 중복 방지

기존에 정상 작동한 DLL은 백업하고, 이 버전은 별도 MO2 테스트 모드에 설치하세요.
