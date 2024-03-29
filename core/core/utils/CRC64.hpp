//
// Created by jglrxavpok on 27/02/2024.
//

#pragma once

// File based on https://www.zlib.net/crc_v3.txt

#include <cstdint>

namespace Carrot {
	// Generated from crctable/gencrc64table.c
	static constexpr const std::uint64_t CRCTable[256] = {
	    0x0ULL, 0x3c3b78e888d80fe1ULL, 0x7876f1d111b01fc2ULL, 0x444d893999681023ULL, 0x750c207570b452a3ULL, 0x4937589df86c5d42ULL, 0xd7ad1a461044d61ULL, 0x3141a94ce9dc4280ULL,
	    0x6ff9833db2bcc861ULL, 0x53c2fbd53a64c780ULL, 0x178f72eca30cd7a3ULL, 0x2bb40a042bd4d842ULL, 0x1af5a348c2089ac2ULL, 0x26cedba04ad09523ULL, 0x62835299d3b88500ULL, 0x5eb82a715b608ae1ULL,
	    0x5a12c5ac36adfde5ULL, 0x6629bd44be75f204ULL, 0x2264347d271de227ULL, 0x1e5f4c95afc5edc6ULL, 0x2f1ee5d94619af46ULL, 0x13259d31cec1a0a7ULL, 0x5768140857a9b084ULL, 0x6b536ce0df71bf65ULL,
	    0x35eb469184113584ULL, 0x9d03e790cc93a65ULL, 0x4d9db74095a12a46ULL, 0x71a6cfa81d7925a7ULL, 0x40e766e4f4a56727ULL, 0x7cdc1e0c7c7d68c6ULL, 0x38919735e51578e5ULL, 0x4aaefdd6dcd7704ULL,
	    0x31c4488f3e8f96edULL, 0xdff3067b657990cULL, 0x49b2b95e2f3f892fULL, 0x7589c1b6a7e786ceULL, 0x44c868fa4e3bc44eULL, 0x78f31012c6e3cbafULL, 0x3cbe992b5f8bdb8cULL, 0x85e1c3d753d46dULL,
	    0x5e3dcbb28c335e8cULL, 0x6206b35a04eb516dULL, 0x264b3a639d83414eULL, 0x1a70428b155b4eafULL, 0x2b31ebc7fc870c2fULL, 0x170a932f745f03ceULL, 0x53471a16ed3713edULL, 0x6f7c62fe65ef1c0cULL,
	    0x6bd68d2308226b08ULL, 0x57edf5cb80fa64e9ULL, 0x13a07cf2199274caULL, 0x2f9b041a914a7b2bULL, 0x1edaad56789639abULL, 0x22e1d5bef04e364aULL, 0x66ac5c8769262669ULL, 0x5a97246fe1fe2988ULL,
	    0x42f0e1eba9ea369ULL, 0x381476f63246ac88ULL, 0x7c59ffcfab2ebcabULL, 0x4062872723f6b34aULL, 0x71232e6bca2af1caULL, 0x4d18568342f2fe2bULL, 0x955dfbadb9aee08ULL, 0x356ea7525342e1e9ULL,
	    0x6388911e7d1f2ddaULL, 0x5fb3e9f6f5c7223bULL, 0x1bfe60cf6caf3218ULL, 0x27c51827e4773df9ULL, 0x1684b16b0dab7f79ULL, 0x2abfc98385737098ULL, 0x6ef240ba1c1b60bbULL, 0x52c9385294c36f5aULL,
	    0xc711223cfa3e5bbULL, 0x304a6acb477bea5aULL, 0x7407e3f2de13fa79ULL, 0x483c9b1a56cbf598ULL, 0x797d3256bf17b718ULL, 0x45464abe37cfb8f9ULL, 0x10bc387aea7a8daULL, 0x3d30bb6f267fa73bULL,
	    0x399a54b24bb2d03fULL, 0x5a12c5ac36adfdeULL, 0x41eca5635a02cffdULL, 0x7dd7dd8bd2dac01cULL, 0x4c9674c73b06829cULL, 0x70ad0c2fb3de8d7dULL, 0x34e085162ab69d5eULL, 0x8dbfdfea26e92bfULL,
	    0x5663d78ff90e185eULL, 0x6a58af6771d617bfULL, 0x2e15265ee8be079cULL, 0x122e5eb66066087dULL, 0x236ff7fa89ba4afdULL, 0x1f548f120162451cULL, 0x5b19062b980a553fULL, 0x67227ec310d25adeULL,
	    0x524cd9914390bb37ULL, 0x6e77a179cb48b4d6ULL, 0x2a3a28405220a4f5ULL, 0x160150a8daf8ab14ULL, 0x2740f9e43324e994ULL, 0x1b7b810cbbfce675ULL, 0x5f3608352294f656ULL, 0x630d70ddaa4cf9b7ULL,
	    0x3db55aacf12c7356ULL, 0x18e224479f47cb7ULL, 0x45c3ab7de09c6c94ULL, 0x79f8d39568446375ULL, 0x48b97ad9819821f5ULL, 0x7482023109402e14ULL, 0x30cf8b0890283e37ULL, 0xcf4f3e018f031d6ULL,
	    0x85e1c3d753d46d2ULL, 0x346564d5fde54933ULL, 0x7028edec648d5910ULL, 0x4c139504ec5556f1ULL, 0x7d523c4805891471ULL, 0x416944a08d511b90ULL, 0x524cd9914390bb3ULL, 0x391fb5719ce10452ULL,
	    0x67a79f00c7818eb3ULL, 0x5b9ce7e84f598152ULL, 0x1fd16ed1d6319171ULL, 0x23ea16395ee99e90ULL, 0x12abbf75b735dc10ULL, 0x2e90c79d3fedd3f1ULL, 0x6add4ea4a685c3d2ULL, 0x56e6364c2e5dcc33ULL,
	    0x42f0e1eba9ea3693ULL, 0x7ecb990321323972ULL, 0x3a86103ab85a2951ULL, 0x6bd68d2308226b0ULL, 0x37fcc19ed95e6430ULL, 0xbc7b97651866bd1ULL, 0x4f8a304fc8ee7bf2ULL, 0x73b148a740367413ULL,
	    0x2d0962d61b56fef2ULL, 0x11321a3e938ef113ULL, 0x557f93070ae6e130ULL, 0x6944ebef823eeed1ULL, 0x580542a36be2ac51ULL, 0x643e3a4be33aa3b0ULL, 0x2073b3727a52b393ULL, 0x1c48cb9af28abc72ULL,
	    0x18e224479f47cb76ULL, 0x24d95caf179fc497ULL, 0x6094d5968ef7d4b4ULL, 0x5cafad7e062fdb55ULL, 0x6dee0432eff399d5ULL, 0x51d57cda672b9634ULL, 0x1598f5e3fe438617ULL, 0x29a38d0b769b89f6ULL,
	    0x771ba77a2dfb0317ULL, 0x4b20df92a5230cf6ULL, 0xf6d56ab3c4b1cd5ULL, 0x33562e43b4931334ULL, 0x217870f5d4f51b4ULL, 0x3e2cffe7d5975e55ULL, 0x7a6176de4cff4e76ULL, 0x465a0e36c4274197ULL,
	    0x7334a9649765a07eULL, 0x4f0fd18c1fbdaf9fULL, 0xb4258b586d5bfbcULL, 0x3779205d0e0db05dULL, 0x6388911e7d1f2ddULL, 0x3a03f1f96f09fd3cULL, 0x7e4e78c0f661ed1fULL, 0x427500287eb9e2feULL,
	    0x1ccd2a5925d9681fULL, 0x20f652b1ad0167feULL, 0x64bbdb88346977ddULL, 0x5880a360bcb1783cULL, 0x69c10a2c556d3abcULL, 0x55fa72c4ddb5355dULL, 0x11b7fbfd44dd257eULL, 0x2d8c8315cc052a9fULL,
	    0x29266cc8a1c85d9bULL, 0x151d14202910527aULL, 0x51509d19b0784259ULL, 0x6d6be5f138a04db8ULL, 0x5c2a4cbdd17c0f38ULL, 0x6011345559a400d9ULL, 0x245cbd6cc0cc10faULL, 0x1867c58448141f1bULL,
	    0x46dfeff5137495faULL, 0x7ae4971d9bac9a1bULL, 0x3ea91e2402c48a38ULL, 0x29266cc8a1c85d9ULL, 0x33d3cf8063c0c759ULL, 0xfe8b768eb18c8b8ULL, 0x4ba53e517270d89bULL, 0x779e46b9faa8d77aULL,
	    0x217870f5d4f51b49ULL, 0x1d43081d5c2d14a8ULL, 0x590e8124c545048bULL, 0x6535f9cc4d9d0b6aULL, 0x54745080a44149eaULL, 0x684f28682c99460bULL, 0x2c02a151b5f15628ULL, 0x1039d9b93d2959c9ULL,
	    0x4e81f3c86649d328ULL, 0x72ba8b20ee91dcc9ULL, 0x36f7021977f9cceaULL, 0xacc7af1ff21c30bULL, 0x3b8dd3bd16fd818bULL, 0x7b6ab559e258e6aULL, 0x43fb226c074d9e49ULL, 0x7fc05a848f9591a8ULL,
	    0x7b6ab559e258e6acULL, 0x4751cdb16a80e94dULL, 0x31c4488f3e8f96eULL, 0x3f273c607b30f68fULL, 0xe66952c92ecb40fULL, 0x325dedc41a34bbeeULL, 0x761064fd835cabcdULL, 0x4a2b1c150b84a42cULL,
	    0x1493366450e42ecdULL, 0x28a84e8cd83c212cULL, 0x6ce5c7b54154310fULL, 0x50debf5dc98c3eeeULL, 0x619f161120507c6eULL, 0x5da46ef9a888738fULL, 0x19e9e7c031e063acULL, 0x25d29f28b9386c4dULL,
	    0x10bc387aea7a8da4ULL, 0x2c87409262a28245ULL, 0x68cac9abfbca9266ULL, 0x54f1b14373129d87ULL, 0x65b0180f9acedf07ULL, 0x598b60e71216d0e6ULL, 0x1dc6e9de8b7ec0c5ULL, 0x21fd913603a6cf24ULL,
	    0x7f45bb4758c645c5ULL, 0x437ec3afd01e4a24ULL, 0x7334a9649765a07ULL, 0x3b08327ec1ae55e6ULL, 0xa499b3228721766ULL, 0x3672e3daa0aa1887ULL, 0x723f6ae339c208a4ULL, 0x4e04120bb11a0745ULL,
	    0x4aaefdd6dcd77041ULL, 0x7695853e540f7fa0ULL, 0x32d80c07cd676f83ULL, 0xee374ef45bf6062ULL, 0x3fa2dda3ac6322e2ULL, 0x399a54b24bb2d03ULL, 0x47d42c72bdd33d20ULL, 0x7bef549a350b32c1ULL,
	    0x25577eeb6e6bb820ULL, 0x196c0603e6b3b7c1ULL, 0x5d218f3a7fdba7e2ULL, 0x611af7d2f703a803ULL, 0x505b5e9e1edfea83ULL, 0x6c6026769607e562ULL, 0x282daf4f0f6ff541ULL, 0x1416d7a787b7faa0ULL,
	};

    constexpr std::uint64_t CRC64(const char* pData, std::size_t length) {
		std::uint64_t crc = -1ull;
		while(length--) {
			crc = CRCTable[((crc ^ *(pData++)) & 0xFF)] ^ (crc >> 8);
		}
        return crc ^ -1ull;
    }
}